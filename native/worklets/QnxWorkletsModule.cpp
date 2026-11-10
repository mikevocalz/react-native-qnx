#include "QnxWorkletsModule.h"
#include "QtWorkletsScheduler.h"
#include "QtWorkletsFrameSource.h"
#include <worklets/NativeModules/WorkletsModuleProxyInitializer.h>
#include <QCoreApplication>
#include <QThread>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace rnqnx::runtime {
namespace jsi = facebook::jsi;
namespace react = facebook::react;
namespace {
// Worklets JSScheduler captures RN Runtime&. Gate before invoking that capture
// so shutdown can cancel accepted RN deliveries without touching dead Hermes.
class GuardedInvoker final : public react::CallInvoker {
 public:
  explicit GuardedInvoker(std::shared_ptr<react::CallInvoker> invoker,
      std::function<void(std::function<void()>)> postToOwner)
      : invoker_(std::move(invoker)), postToOwner_(std::move(postToOwner)), alive_(std::make_shared<std::atomic<bool>>(true)) {}
  void invokeAsync(react::CallFunc&& function) noexcept override {
    std::lock_guard lock(admission_);
    const auto alive = alive_;
    if (!alive->load()) return;
    // RN's synchronous host invoker is not a cross-thread dispatcher. Always
    // enter through the existing owner queue before consulting the RN runtime.
    postToOwner_([alive, invoker = invoker_, function = std::move(function)]() mutable {
      if (!alive->load()) return;
      invoker->invokeAsync([alive, function = std::move(function)](jsi::Runtime& runtime) {
        if (alive->load()) function(runtime);
      });
    });
  }
  void invokeSync(react::CallFunc&&) override {
    throw std::logic_error("Worklets synchronous cross-thread RN invocation is unsupported");
  }
  void close() { std::lock_guard lock(admission_); alive_->store(false); }
 private:
  std::mutex admission_;
  std::shared_ptr<react::CallInvoker> invoker_;
  std::function<void(std::function<void()>)> postToOwner_;
  std::shared_ptr<std::atomic<bool>> alive_;
};
}
struct QnxWorkletsModule::Impl {
  Impl(jsi::Runtime& runtime, std::shared_ptr<react::CallInvoker> invoker,
      std::function<void(std::function<void()>)> postToOwner)
      : runtime(runtime), owner(QThread::currentThread()), guarded(std::make_shared<GuardedInvoker>(std::move(invoker), std::move(postToOwner))),
        status(std::make_shared<worklets::RNRuntimeStatus>()) {
    if (!QCoreApplication::instance() || owner != QCoreApplication::instance()->thread()) {
      throw std::logic_error("Worklets bootstrap requires the Qt/Hermes owner");
    }
    scheduler = std::make_shared<QtWorkletsScheduler>([invoker = guarded](std::string message) {
      invoker->invokeAsync([message = std::move(message)](jsi::Runtime& rt) {
        throw jsi::JSError(rt, message);
      });
    });
    jsScheduler = std::make_shared<worklets::JSScheduler>(runtime, guarded,
        [thread = owner] { return QThread::currentThread() == thread; });
    frames = std::make_shared<QtWorkletsFrameSource>(scheduler);
    bindings = std::make_shared<worklets::RuntimeBindings>(worklets::RuntimeBindings{
        .requestAnimationFrame = [source = std::weak_ptr<QtWorkletsFrameSource>(frames)](std::function<void(double)> callback) {
          const auto frames = source.lock();
          if (!frames) throw std::runtime_error("Worklets frame source is closed");
          frames->request(std::move(callback));
        },
        .nativeLoggingHook = {},
        .networkingBackend = nullptr});
    initializer = std::make_unique<worklets::WorkletsModuleProxyInitializer>(
        jsScheduler, scheduler, bindings, status);
    // Startup only, before application bundle evaluation. No blocking future
    // is left for installTurboModule to wait on while the app is running.
    initializer->prepareProxy();
  }
  void requireOwner() const { if (QThread::currentThread() != owner) std::terminate(); }
  jsi::Runtime& runtime;
  QThread* owner;
  std::shared_ptr<GuardedInvoker> guarded;
  std::shared_ptr<worklets::RNRuntimeStatus> status;
  std::shared_ptr<QtWorkletsScheduler> scheduler;
  std::shared_ptr<QtWorkletsFrameSource> frames;
  std::shared_ptr<worklets::JSScheduler> jsScheduler;
  std::shared_ptr<worklets::RuntimeBindings> bindings;
  std::unique_ptr<worklets::WorkletsModuleProxyInitializer> initializer;
  std::shared_ptr<worklets::WorkletsModuleProxy> proxy;
};
QnxWorkletsModule::QnxWorkletsModule(jsi::Runtime& runtime, std::shared_ptr<react::CallInvoker> invoker,
    std::function<void(std::function<void()>)> postToOwner)
    : react::TurboModule(kModuleName, invoker), impl_(std::make_unique<Impl>(runtime, std::move(invoker), std::move(postToOwner))) {
  methodMap_["installTurboModule"] = MethodMetadata{1, install};
  methodMap_["start"] = MethodMetadata{0, start};
  methodMap_["prepareBundleMode"] = MethodMetadata{0, unsupported};
  methodMap_["toggleSlowAnimationsOnUIRuntime"] = MethodMetadata{0, unsupported};
}
QnxWorkletsModule::~QnxWorkletsModule() { invalidate(); }
void QnxWorkletsModule::invalidate() {
  if (!impl_) return;
  impl_->requireOwner();
  impl_->guarded->close();
  impl_->status->setDead();
  impl_->frames->close();
  impl_->scheduler->close();
  impl_->initializer->invalidate();
  // The app may freeze the global property. Do not mutate app-owned JS during
  // noexcept teardown; the closed status/queue gates reject late deliveries.
  // The RN runtime releases its proxy reference during its own destruction.
  impl_.reset();
}
void QnxWorkletsModule::setWindow(QQuickWindow* window) {
  if (!impl_) throw std::logic_error("Worklets module is closed");
  impl_->requireOwner();
  impl_->frames->attach(window);
}
jsi::Value QnxWorkletsModule::install(jsi::Runtime& runtime, react::TurboModule& module,
    const jsi::Value* args, size_t count) {
  auto& state = static_cast<QnxWorkletsModule&>(module).impl_;
  if (!state) throw jsi::JSError(runtime, "Worklets observation is closed");
  state->requireOwner();
  if (count != 1 || !args[0].isBool() || args[0].getBool()) {
    throw jsi::JSError(runtime, "QNX Worklets prototype requires non-Bundle Mode");
  }
  if (!state->frames->ready()) throw jsi::JSError(runtime, "Attach Worklets QQuickWindow before loading the bundle");
  if (!state->proxy) {
    state->proxy = state->initializer->finalize(runtime, false, [] { return worklets::BundleModeConfig{.enabled = false}; });
  }
  return jsi::Value(true);
}
jsi::Value QnxWorkletsModule::start(jsi::Runtime& runtime, react::TurboModule& module,
    const jsi::Value*, size_t count) {
  auto& state = static_cast<QnxWorkletsModule&>(module).impl_;
  if (!state || !state->proxy || count != 0) throw jsi::JSError(runtime, "Install Worklets before start");
  state->requireOwner();
  state->proxy->start();
  return jsi::Value(true);
}
jsi::Value QnxWorkletsModule::unsupported(jsi::Runtime& runtime, react::TurboModule&,
    const jsi::Value*, size_t) {
  throw jsi::JSError(runtime, "QNX Worklets prototype does not support Bundle Mode or slow animations");
}
}
