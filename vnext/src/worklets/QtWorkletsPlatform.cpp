#include "react-native-qnx/worklets/QtWorkletsPlatform.h"

#include "QnxWorkletsModule.h"
#include "QtTaskQueueCallInvoker.h"
#include "QtUIScheduler.h"
#include "react-native-qnx/worklets/QtFrameBridge.h"
#include "react-native-qnx/worklets/QtUiThread.h"

#include <worklets/NativeModules/WorkletsModuleProxyInitializer.h>
#include <worklets/Tools/JSScheduler.h>
#include <worklets/Tools/RNRuntimeStatus.h>
#include <worklets/WorkletRuntime/RuntimeBindings.h>

#include <QQuickWindow>

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

namespace rnqnx::worklets {

namespace {

class Platform final : public QtWorkletsPlatform,
                       public std::enable_shared_from_this<Platform> {
 public:
  Platform(runtime::QtReactHost& host, QQuickWindow* window)
      : guiThread_(std::this_thread::get_id()),
        stats_(std::make_shared<QtWorkletsStats>()),
        uiThread_(std::make_shared<QtUiThread>(QStringLiteral("rnqnx-ui-worklet"))),
        frames_(std::make_shared<QtFrameBridge>(window, *uiThread_)),
        callInvoker_(std::make_shared<QtTaskQueueCallInvoker>(host.taskQueue(), stats_, host.runtime())),
        rnRuntimeStatus_(std::make_shared<::worklets::RNRuntimeStatus>()),
        host_(&host) {}

  /// Second phase: needs weak_from_this().
  void init() {
    uiScheduler_ = std::make_shared<QtUIScheduler>(uiThread_, stats_);
    auto weak = weak_from_this();
    auto runtimeBindings = std::make_shared<::worklets::RuntimeBindings>(
        ::worklets::RuntimeBindings{
            .requestAnimationFrame =
                [weak](std::function<void(const double)> callback) {
                  if (auto self = weak.lock()) {
                    self->stats_->framesRequested.fetch_add(1, std::memory_order_relaxed);
                    self->frames_->request(std::move(callback));
                  }
                },
            .nativeLoggingHook = &logToStderr,
            .networkingBackend = nullptr});
    auto jsScheduler = std::make_shared<::worklets::JSScheduler>(
        host_->runtime(), callInvoker_, [this]() { return std::this_thread::get_id() == guiThread_; });
    initializer_ = std::make_shared<::worklets::WorkletsModuleProxyInitializer>(
        jsScheduler, uiScheduler_, runtimeBindings, rnRuntimeStatus_);
    // The proxy constructor creates the UI runtime without initializing it.
    initializer_->prepareProxy();
  }

  ~Platform() override { shutdown(); }

  const QtWorkletsStats& stats() const noexcept override { return *stats_; }
  bool uiThreadIsCurrent() const noexcept override { return uiThread_->isCurrent(); }
  std::string currentThreadRole() const override {
    if (std::this_thread::get_id() == guiThread_) return "gui";
    if (uiThread_->isCurrent()) return "ui-worklet";
    return "other";
  }

  void requestFrame(FrameCallback callback) override {
    stats_->framesRequested.fetch_add(1, std::memory_order_relaxed);
    frames_->request(std::move(callback));
  }
  std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker() const override { return callInvoker_; }
  bool postToUi(std::function<void()> job) override { return uiThread_->post(std::move(job)); }
  void postToGui(std::function<void()> job) override { frames_->postToGui(std::move(job)); }
  double animationTimestampMs() const override {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  void shutdown() override {
    if (down_.exchange(true)) {
      return;
    }
    // Order: stop new frames, mark the RN runtime dead so queued UI work stops
    // calling back into it, invalidate the proxy, then join the UI thread. The
    // UI runtime object itself lives on until the RN runtime drops its handle.
    frames_->shutdown();
    rnRuntimeStatus_->setDead();
    if (module_) {
      module_->invalidate();
    } else if (initializer_) {
      initializer_->invalidate();
    }
    module_.reset();
    initializer_.reset();
    uiThread_->stop();
    stats_->framesDelivered.store(frames_->framesDelivered());
    std::fprintf(stderr,
                 "RNQNX_WORKLETS_SHUTDOWN_OK framesRequested=%llu framesDelivered=%llu uiJobs=%llu jsPosts=%llu\n",
                 static_cast<unsigned long long>(stats_->framesRequested.load()),
                 static_cast<unsigned long long>(stats_->framesDelivered.load()),
                 static_cast<unsigned long long>(stats_->uiJobsRun.load()),
                 static_cast<unsigned long long>(stats_->jsPosts.load()));
    std::fflush(stderr);
  }

  std::shared_ptr<facebook::react::TurboModule> makeModule() {
    if (down_.load()) {
      return nullptr;
    }
    if (!module_) {
      module_ = std::make_shared<QnxWorkletsModule>(
          callInvoker_, initializer_,
          [weak = weak_from_this()](facebook::jsi::Runtime& rt) {
            if (auto self = weak.lock()) {
              self->installProbeHelpers(rt);
            }
          });
    }
    return module_;
  }

  /// Host functions on the UI runtime that report which thread and how many
  /// frames. Worklets cannot see the RN runtime's globals, so they live here.
  void installProbeHelpers(facebook::jsi::Runtime& rt) {
    auto weak = weak_from_this();
    rt.global().setProperty(
        rt, "__rnqnxThreadRole",
        facebook::jsi::Function::createFromHostFunction(
            rt, facebook::jsi::PropNameID::forAscii(rt, "__rnqnxThreadRole"), 0,
            [weak](facebook::jsi::Runtime& r, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) {
              auto self = weak.lock();
              return facebook::jsi::String::createFromUtf8(r, self ? self->currentThreadRole() : "gone");
            }));
    rt.global().setProperty(
        rt, "__rnqnxLog",
        facebook::jsi::Function::createFromHostFunction(
            rt, facebook::jsi::PropNameID::forAscii(rt, "__rnqnxLog"), 1,
            [weak](facebook::jsi::Runtime& r, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t count) {
              if (count >= 1 && args[0].isString()) {
                auto self = weak.lock();
                std::fprintf(stderr, "RNQNX_JS_LOG [%s] %s\n",
                             self ? self->currentThreadRole().c_str() : "gone",
                             args[0].getString(r).utf8(r).c_str());
                std::fflush(stderr);
              }
              return facebook::jsi::Value::undefined();
            }));
    rt.global().setProperty(
        rt, "__rnqnxFrameStats",
        facebook::jsi::Function::createFromHostFunction(
            rt, facebook::jsi::PropNameID::forAscii(rt, "__rnqnxFrameStats"), 0,
            [weak](facebook::jsi::Runtime& r, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) {
              facebook::jsi::Object out(r);
              if (auto self = weak.lock()) {
                out.setProperty(r, "requested", static_cast<double>(self->stats_->framesRequested.load()));
                out.setProperty(r, "delivered", static_cast<double>(self->frames_->framesDelivered()));
                out.setProperty(r, "signalled", static_cast<double>(self->frames_->framesSignalled()));
                out.setProperty(r, "uiJobs", static_cast<double>(self->stats_->uiJobsRun.load()));
              }
              return facebook::jsi::Value(std::move(out));
            }));
  }

 private:
  static facebook::jsi::Value logToStderr(
      facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
      const facebook::jsi::Value* args, size_t count) {
    if (count >= 1 && args[0].isString()) {
      std::fprintf(stderr, "[worklet] %s\n", args[0].getString(rt).utf8(rt).c_str());
    }
    return facebook::jsi::Value::undefined();
  }

  const std::thread::id guiThread_;
  std::shared_ptr<QtWorkletsStats> stats_;
  std::shared_ptr<QtUiThread> uiThread_;
  std::shared_ptr<QtFrameBridge> frames_;
  std::shared_ptr<QtTaskQueueCallInvoker> callInvoker_;
  std::shared_ptr<::worklets::RNRuntimeStatus> rnRuntimeStatus_;
  std::shared_ptr<QtUIScheduler> uiScheduler_;
  std::shared_ptr<::worklets::WorkletsModuleProxyInitializer> initializer_;
  std::shared_ptr<QnxWorkletsModule> module_;
  runtime::QtReactHost* host_;
  std::atomic<bool> down_{false};
};

}  // namespace

std::shared_ptr<QtWorkletsPlatform> QtWorkletsPlatform::install(
    runtime::QtReactHost& host, QQuickWindow* window) {
  auto platform = std::make_shared<Platform>(host, window);
  platform->init();
  host.registerTurboModule(
      QnxWorkletsModule::kModuleName,
      [platform](std::shared_ptr<facebook::react::CallInvoker>) {
        return platform->makeModule();
      });
  // The UI-worklet thread may commit to the shadow tree (Reanimated does); the
  // mounting that follows must run on this, the GUI, thread.
  host.setOwnerPoster([weak = std::weak_ptr<QtWorkletsPlatform>(platform)](std::function<void()> job) {
    if (auto self = weak.lock()) {
      self->postToGui(std::move(job));
    }
  });
  host.addTeardownHook([platform]() { platform->shutdown(); });
  return platform;
}

}  // namespace rnqnx::worklets
