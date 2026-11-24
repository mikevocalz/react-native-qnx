#include "react-native-qnx/runtime/QnxAppStateModule.h"
#include "QtApplicationLifecycle.h"
#include "HostWindowState.h"

#include <QDebug>
#include <QGuiApplication>
#include <QThread>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
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

// JavaScript has one background state, so Hidden and Suspended are the same cached value.
ApplicationVisibility canonical(ApplicationVisibility visibility) {
  return visibility == ApplicationVisibility::Suspended ? ApplicationVisibility::Hidden : visibility;
}

void arguments(jsi::Runtime& runtime, size_t count, size_t expected) {
  if (count != expected) throw jsi::JSError(runtime, "AppState: invalid argument count");
}
}  // namespace

// The Qt observer lives on the QGuiApplication owner (GUI) thread and is only
// touched there. JavaScript reads `current`, an atomic cache. With the
// JavaScript thread on its own thread (ADR-0021) the module is built and
// called on that thread, the observer is made and destroyed by a task posted
// to the GUI thread, and delivery goes through the thread-safe invokeAsync.
struct QnxAppStateModule::Impl {
  struct GuiSide {
    LifecycleObserverPtr observer;
  };

  Impl(QGuiApplication& app, bool jsOnOwner)
      : owner(app.thread()), jsOnOwner(jsOnOwner), gui(std::make_shared<GuiSide>()) {}

  void requireOwner() const noexcept {
    // A wrong-thread destruction is an invariant failure, not permission to
    // defer disposal past the runtime.
    if (QThread::currentThread() != owner) std::terminate();
  }
  // Ordinary JS calls share the owner thread unless JS has its own thread.
  void requireJs() const noexcept {
    if (jsOnOwner) requireOwner();
  }
  void requireActive(jsi::Runtime& runtime) const {
    requireJs();
    if (!active.load(std::memory_order_acquire)) throw jsi::JSError(runtime, "AppState: observation is closed");
  }
  std::string currentName() const {
    return appState(static_cast<ApplicationVisibility>(current.load(std::memory_order_acquire)));
  }

  QThread* const owner;
  const bool jsOnOwner;
  std::shared_ptr<GuiSide> gui;
  std::atomic<int> current{static_cast<int>(ApplicationVisibility::Unknown)};
  std::atomic<bool> active{true};
  std::atomic<size_t> listeners{0};
};

QnxAppStateModule::QnxAppStateModule(std::shared_ptr<react::CallInvoker> jsInvoker)
    : react::TurboModule(kModuleName, jsInvoker) {
  auto* application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
  if (!application || !jsInvoker) {
    throw std::logic_error("AppState requires the QGuiApplication and a runtime invoker");
  }
  const bool onOwner = QThread::currentThread() == application->thread();
  impl_ = std::make_shared<Impl>(*application, onOwner);
  const std::weak_ptr<Impl> weak = impl_;

  // Runs on the GUI thread: applies a visibility and, when it changed, tells JS.
  const auto deliver = [weak, jsInvoker](ApplicationVisibility visibility) {
    const auto state = weak.lock();
    if (!state || !state->active.load(std::memory_order_acquire)) return;
    state->requireOwner();
    visibility = canonical(visibility);
    if (state->current.exchange(static_cast<int>(visibility), std::memory_order_acq_rel) ==
        static_cast<int>(visibility))
      return;
    const auto next = appState(visibility);
    // The existing RuntimeSchedulerCallInvoker is already the owned JS
    // route (invokeAsync is thread safe). Do not add a worker or another
    // queue hop, retain a Runtime&, capture this module, or keep any JSI value
    // in a native event.
    jsInvoker->invokeAsync([weak, next](jsi::Runtime& runtime) {
      const auto live = weak.lock();
      if (!live || !live->active.load(std::memory_order_acquire)) return;
      live->requireJs();
      auto emitter = runtime.global().getProperty(runtime, "__rctDeviceEventEmitter");
      if (!emitter.isObject()) return;
      auto object = emitter.asObject(runtime);
      auto payload = jsi::Object(runtime);
      payload.setProperty(runtime, "app_state", jsi::String::createFromUtf8(runtime, next));
      object.getPropertyAsFunction(runtime, "emit").callWithThis(runtime, object,
          jsi::String::createFromAscii(runtime, "appStateDidChange"), std::move(payload));
    });
  };

  // Runs on the GUI thread.
  const auto attach = [weak, deliver, application]() {
    const auto state = weak.lock();
    if (!state || !state->active.load(std::memory_order_acquire)) return;
    state->requireOwner();
    LifecycleObserverPtr observer(new QtApplicationLifecycle(*application));
    QtApplicationLifecycle* raw = observer.get();
    QObject::connect(raw, &QtApplicationLifecycle::stateChanged, raw,
        [deliver](LifecycleSnapshot snapshot) { deliver(snapshot.visibility); });
    QObject::connect(raw, &QtApplicationLifecycle::observationClosed, raw, [weak] {
      if (const auto live = weak.lock()) live->active.store(false, std::memory_order_release);
    });
    state->gui->observer = std::move(observer);
    // Connect first, then read the snapshot: a change that came in while the
    // observer was being posted is delivered here.
    deliver(raw->snapshot().visibility);
  };

  if (onOwner) {
    // The first value is exact and synchronous, as before.
    LifecycleObserverPtr observer(new QtApplicationLifecycle(*application));
    impl_->current.store(static_cast<int>(canonical(observer->snapshot().visibility)), std::memory_order_release);
    impl_->gui->observer = std::move(observer);
    QtApplicationLifecycle* raw = impl_->gui->observer.get();
    QObject::connect(raw, &QtApplicationLifecycle::stateChanged, raw,
        [deliver](LifecycleSnapshot snapshot) { deliver(snapshot.visibility); });
    QObject::connect(raw, &QtApplicationLifecycle::observationClosed, raw, [weak] {
      if (const auto live = weak.lock()) live->active.store(false, std::memory_order_release);
    });
  } else {
    // JavaScript has its own thread: no Qt object is made or read here. The
    // first value comes from the host's atomics, the observer follows from the
    // GUI thread's event loop and corrects it if anything moved meanwhile.
    auto& host = HostWindowState::instance();
    impl_->current.store(static_cast<int>(host.minimized()
                             ? ApplicationVisibility::Hidden
                             : canonical(QtApplicationLifecycle::visibilityFor(host.qtApplicationState()))),
        std::memory_order_release);
    QMetaObject::invokeMethod(application, attach, Qt::QueuedConnection);
  }
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
  // From here no delivery is accepted, on any thread.
  impl_->active.store(false, std::memory_order_release);
  impl_->listeners.store(0, std::memory_order_release);
  const auto gui = impl_->gui;
  const auto release = [gui] {
    if (!gui->observer) return;
    gui->observer->close();
    gui->observer.reset();
  };
  if (QThread::currentThread() == impl_->owner) {
    // QtReactHost invalidates the observer on the application owner thread
    // before the scheduler closes. TurboModule registry references may be
    // released later on a different scheduler/GC thread; by then there is
    // nothing left to touch.
    release();
  } else if (auto* application = QCoreApplication::instance()) {
    // JavaScript thread: the Qt observer belongs to the GUI thread, which may
    // be blocked waiting for this one, so never wait for it. The posted task
    // runs after the attach task, which then finds the module inactive.
    QMetaObject::invokeMethod(application, release, Qt::QueuedConnection);
  }
}

jsi::Value QnxAppStateModule::getConstants(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value*, size_t count) {
  arguments(runtime, count, 0);
  const auto state = static_cast<QnxAppStateModule&>(module).impl_;
  state->requireActive(runtime);
  auto constants = jsi::Object(runtime);
  constants.setProperty(runtime, "initialAppState", jsi::String::createFromUtf8(runtime, state->currentName()));
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
  snapshot.setProperty(runtime, "app_state", jsi::String::createFromUtf8(runtime, state->currentName()));
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
  if (state->listeners.load() == std::numeric_limits<size_t>::max()) {
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
  state->requireJs();
  if (!args[0].isNumber() || !std::isfinite(args[0].getNumber()) ||
      args[0].getNumber() < 0 || std::floor(args[0].getNumber()) != args[0].getNumber() ||
      args[0].getNumber() > 9007199254740991.0) {
    throw jsi::JSError(runtime, "AppState: listener removal count must be a nonnegative safe integer");
  }
  const double requested = args[0].getNumber();
  if (requested >= static_cast<double>(state->listeners.load())) state->listeners = 0;
  else state->listeners -= static_cast<size_t>(requested);
  return jsi::Value::undefined();
}

}  // namespace rnqnx::runtime
