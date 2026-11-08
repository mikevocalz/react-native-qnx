#include "react-native-qnx/runtime/QnxAppStateModule.h"
#include "QtApplicationLifecycle.h"

#include <QDebug>
#include <QGuiApplication>
#include <QThread>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace rnqnx::runtime {
namespace jsi = facebook::jsi;
namespace react = facebook::react;

namespace {
std::string appState(ApplicationVisibility visibility) {
  switch (visibility) {
    case ApplicationVisibility::Active: return "active";
    case ApplicationVisibility::Inactive: return "inactive";
    case ApplicationVisibility::Hidden:
    case ApplicationVisibility::Suspended: return "background";
    case ApplicationVisibility::Unknown: return "unknown";
  }
  return "unknown";
}

void arguments(jsi::Runtime& runtime, size_t count, size_t expected) {
  if (count != expected) throw jsi::JSError(runtime, "AppState: invalid argument count");
}
}  // namespace

struct QnxAppStateModule::Impl {
  explicit Impl(QGuiApplication& app)
      : owner(app.thread()), observer(std::make_unique<QtApplicationLifecycle>(app)),
        current(appState(observer->snapshot().visibility)) {}

  void requireOwner() const noexcept {
    // A wrong-thread destruction is an invariant failure, not permission to
    // defer disposal past the runtime. Ordinary JS calls share this owner.
    if (QThread::currentThread() != owner) std::terminate();
  }
  void requireActive(jsi::Runtime& runtime) const {
    requireOwner();
    if (!active) throw jsi::JSError(runtime, "AppState: observation is closed");
  }

  QThread* const owner;
  std::unique_ptr<QtApplicationLifecycle> observer;
  std::string current;
  bool active{true};
  size_t listeners{0};
};

QnxAppStateModule::QnxAppStateModule(std::shared_ptr<react::CallInvoker> jsInvoker)
    : react::TurboModule(kModuleName, jsInvoker) {
  auto* application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
  if (!application || QThread::currentThread() != application->thread() || !jsInvoker) {
    throw std::logic_error("AppState requires the QGuiApplication owner and a runtime invoker");
  }
  impl_ = std::make_shared<Impl>(*application);
  const std::weak_ptr<Impl> weak = impl_;
  QObject::connect(impl_->observer.get(), &QtApplicationLifecycle::stateChanged,
      impl_->observer.get(), [weak, jsInvoker](LifecycleSnapshot snapshot) {
        const auto state = weak.lock();
        if (!state || !state->active) return;
        state->requireOwner();
        const auto next = appState(snapshot.visibility);
        if (next == state->current) return;
        state->current = next;
        // The existing RuntimeSchedulerCallInvoker is already the owned JS
        // route. Do not add a worker or another queue hop, retain a Runtime&,
        // capture this module, or keep any JSI value in a native event.
        jsInvoker->invokeAsync([weak, next](jsi::Runtime& runtime) {
          const auto live = weak.lock();
          if (!live || !live->active) return;
          live->requireOwner();
          auto emitter = runtime.global().getProperty(runtime, "__rctDeviceEventEmitter");
          if (!emitter.isObject()) return;
          auto object = emitter.asObject(runtime);
          auto payload = jsi::Object(runtime);
          payload.setProperty(runtime, "app_state", jsi::String::createFromUtf8(runtime, next));
          object.getPropertyAsFunction(runtime, "emit").callWithThis(runtime, object,
              jsi::String::createFromAscii(runtime, "appStateDidChange"), std::move(payload));
        });
      });
  QObject::connect(impl_->observer.get(), &QtApplicationLifecycle::observationClosed,
      impl_->observer.get(), [weak] {
        if (const auto state = weak.lock()) state->active = false;
      });
  methodMap_["getConstants"] = MethodMetadata{0, getConstants};
  methodMap_["getCurrentAppState"] = MethodMetadata{2, getCurrentAppState};
  methodMap_["addListener"] = MethodMetadata{1, addListener};
  methodMap_["removeListeners"] = MethodMetadata{1, removeListeners};
}

QnxAppStateModule::~QnxAppStateModule() {
  invalidate();
}

void QnxAppStateModule::invalidate() noexcept {
  if (!impl_) return;
  impl_->requireOwner();
  impl_->active = false;
  impl_->listeners = 0;
  if (impl_->observer) {
    impl_->observer->close();
    impl_->observer.reset();
  }
}

jsi::Value QnxAppStateModule::getConstants(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value*, size_t count) {
  arguments(runtime, count, 0);
  const auto state = static_cast<QnxAppStateModule&>(module).impl_;
  state->requireActive(runtime);
  auto constants = jsi::Object(runtime);
  constants.setProperty(runtime, "initialAppState", jsi::String::createFromUtf8(runtime, state->current));
  return constants;
}

jsi::Value QnxAppStateModule::getCurrentAppState(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value* args, size_t count) {
  arguments(runtime, count, 2);
  const auto state = static_cast<QnxAppStateModule&>(module).impl_;
  state->requireActive(runtime);
  if (!args[0].isObject() || !args[0].asObject(runtime).isFunction(runtime) ||
      !args[1].isObject() || !args[1].asObject(runtime).isFunction(runtime)) {
    throw jsi::JSError(runtime, "AppState: expected success and error callbacks");
  }
  auto snapshot = jsi::Object(runtime);
  snapshot.setProperty(runtime, "app_state", jsi::String::createFromUtf8(runtime, state->current));
  // This cache is cheap and synchronous; no callback is retained natively.
  args[0].asObject(runtime).asFunction(runtime).call(runtime, std::move(snapshot));
  return jsi::Value::undefined();
}

jsi::Value QnxAppStateModule::addListener(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value* args, size_t count) {
  arguments(runtime, count, 1);
  const auto state = static_cast<QnxAppStateModule&>(module).impl_;
  state->requireActive(runtime);
  if (!args[0].isString() || args[0].asString(runtime).utf8(runtime) != "appStateDidChange") {
    throw jsi::JSError(runtime, "AppState: only appStateDidChange has a native provider");
  }
  if (state->listeners == std::numeric_limits<size_t>::max()) {
    throw jsi::JSError(runtime, "AppState: listener count overflow");
  }
  ++state->listeners;
  // RN's actual non-iOS AppState intentionally supplies null to NativeEventEmitter.
  // Its permanent currentState listener does not call addListener/removeListeners;
  // using this count to gate Qt observation would silently drop real RN events.
  return jsi::Value::undefined();
}

jsi::Value QnxAppStateModule::removeListeners(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value* args, size_t count) {
  arguments(runtime, count, 1);
  const auto state = static_cast<QnxAppStateModule&>(module).impl_;
  state->requireOwner();
  if (!args[0].isNumber() || !std::isfinite(args[0].getNumber()) ||
      args[0].getNumber() < 0 || std::floor(args[0].getNumber()) != args[0].getNumber() ||
      args[0].getNumber() > 9007199254740991.0) {
    throw jsi::JSError(runtime, "AppState: listener removal count must be a nonnegative safe integer");
  }
  const double requested = args[0].getNumber();
  if (requested >= static_cast<double>(state->listeners)) state->listeners = 0;
  else state->listeners -= static_cast<size_t>(requested);
  return jsi::Value::undefined();
}

}  // namespace rnqnx::runtime
