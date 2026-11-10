#include "react-native-qnx/runtime/QtDeviceEventManagerModule.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QPointer>
#include <QQuickWindow>
#include <QThread>
#include <functional>
#include <stdexcept>

namespace rnqnx::runtime {
namespace jsi = facebook::jsi;
namespace react = facebook::react;

namespace {
class QtBackEventFilter final : public QObject {
 public:
  explicit QtBackEventFilter(std::function<bool(QObject*, QEvent*)> callback) : callback_(std::move(callback)) {
    qGuiApp->installEventFilter(this);
  }
  ~QtBackEventFilter() override { qGuiApp->removeEventFilter(this); }
  bool eventFilter(QObject* watched, QEvent* event) override { return callback_(watched, event); }
 private:
  std::function<bool(QObject*, QEvent*)> callback_;
};
}  // namespace

struct QtDeviceEventManagerModule::Impl final : std::enable_shared_from_this<Impl> {
  Impl(std::shared_ptr<react::CallInvoker> route, fabric::ShadowViewMutationAdapter& views)
      : adapter(views), invoker(std::move(route)), owner(QThread::currentThread()) {}
  void start() {
    const std::weak_ptr<Impl> weak = shared_from_this();
    filter = std::make_unique<QtBackEventFilter>([weak](QObject* watched, QEvent* event) {
      const auto state = weak.lock();
      return state && state->active && state->onEvent(watched, event);
    });
  }
  void requireOwner() const noexcept {
    if (QThread::currentThread() != owner) std::terminate();
  }
  void close() noexcept {
    if (!active) return;
    requireOwner();
    active = false;
    filter.reset();
  }
  void defaultBack() {
    requireOwner();
    auto* window = adapter.hostWindow();
    if (!window) throw std::runtime_error("BackHandler: no live host window");
    // Closing can destroy the embedding host. Leave the current JS call before
    // asking Qt to close, and let the window's normal close policy decide.
    const QPointer<QQuickWindow> target(window);
    const std::weak_ptr<Impl> weak = shared_from_this();
    QMetaObject::invokeMethod(window, [weak, target] {
      const auto state = weak.lock();
      if (state && state->active && target) target->close();
    }, Qt::QueuedConnection);
  }
  bool onEvent(QObject* watched, QEvent* event) {
    if (!active || watched != adapter.hostWindow() ||
        (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)) return false;
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() != Qt::Key_Back || key->modifiers() != Qt::NoModifier) return false;
    key->accept();
    // One press/release pair yields one action. Ignore auto-repeat releases.
    if (event->type() != QEvent::KeyRelease || key->isAutoRepeat()) return true;
    const std::weak_ptr<Impl> weak = shared_from_this();
    const double timestamp = static_cast<double>(key->timestamp());
    invoker->invokeAsync([weak, timestamp](jsi::Runtime& runtime) {
      const auto state = weak.lock();
      if (!state || !state->active) return;
      state->requireOwner();
      // A modal owns system-back until it unmounts, matching RN's contract.
      if (state->adapter.requestModalClose()) return;
      auto emitter = runtime.global().getProperty(runtime, "__rctDeviceEventEmitter");
      if (!emitter.isObject()) {
        state->defaultBack();
        return;
      }
      auto object = emitter.asObject(runtime);
      const auto listeners = object.getPropertyAsFunction(runtime, "listenerCount").callWithThis(
          runtime, object, jsi::String::createFromAscii(runtime, "hardwareBackPress"));
      if (listeners.isNumber() && listeners.getNumber() == 0) {
        state->defaultBack();
        return;
      }
      auto payload = jsi::Object(runtime);
      payload.setProperty(runtime, "timeStamp", timestamp);
      object.getPropertyAsFunction(runtime, "emit").callWithThis(runtime, object,
          jsi::String::createFromAscii(runtime, "hardwareBackPress"), std::move(payload));
    });
    return true;
  }
  fabric::ShadowViewMutationAdapter& adapter;
  const std::shared_ptr<react::CallInvoker> invoker;
  QThread* const owner;
  std::unique_ptr<QtBackEventFilter> filter;
  bool active{true};
};

QtDeviceEventManagerModule::QtDeviceEventManagerModule(std::shared_ptr<react::CallInvoker> invoker,
    fabric::ShadowViewMutationAdapter& adapter) : react::TurboModule(kModuleName, invoker) {
  if (!qGuiApp || QThread::currentThread() != qGuiApp->thread() || !invoker) {
    throw std::logic_error("BackHandler requires the Qt application owner and runtime invoker");
  }
  impl_ = std::make_shared<Impl>(std::move(invoker), adapter);
  impl_->start();
  methodMap_["invokeDefaultBackPressHandler"] = MethodMetadata{0, invokeDefaultBackPressHandler};
}

QtDeviceEventManagerModule::~QtDeviceEventManagerModule() { invalidate(); }
void QtDeviceEventManagerModule::invalidate() noexcept {
  if (impl_) impl_->close();
}

jsi::Value QtDeviceEventManagerModule::invokeDefaultBackPressHandler(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value*, size_t count) {
  const auto state = static_cast<QtDeviceEventManagerModule&>(module).impl_;
  state->requireOwner();
  if (!state->active) throw jsi::JSError(runtime, "BackHandler: observation is closed");
  if (count != 0) throw jsi::JSError(runtime, "BackHandler: default action takes no arguments");
  try {
    state->defaultBack();
  } catch (const std::exception& error) {
    throw jsi::JSError(runtime, error.what());
  }
  return jsi::Value::undefined();
}

}  // namespace rnqnx::runtime
