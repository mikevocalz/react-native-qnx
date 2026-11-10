#include "react-native-qnx/runtime/QtAccessibilityInfoModule.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"

#include <QAccessible>
#include <QAccessibilityHints>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStyleHints>
#include <QThread>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace rnqnx::runtime {
namespace jsi = facebook::jsi;
namespace react = facebook::react;

namespace {
class QtAccessibilityObserver final : public QObject, public QAccessible::ActivationObserver {
 public:
  explicit QtAccessibilityObserver(std::function<void(bool)> callback) : callback_(std::move(callback)) {
    QAccessible::installActivationObserver(this);
  }
  ~QtAccessibilityObserver() override { QAccessible::removeActivationObserver(this); }
  void accessibilityActiveChanged(bool active) override { callback_(active); }
 private:
  std::function<void(bool)> callback_;
};
}  // namespace

struct QtAccessibilityInfoModule::Impl final : std::enable_shared_from_this<Impl> {
  Impl(std::shared_ptr<react::CallInvoker> invoker, fabric::ShadowViewMutationAdapter& views)
      : adapter(views), invoker(std::move(invoker)), owner(QThread::currentThread()) {}

  void start() {
    const std::weak_ptr<Impl> weak = shared_from_this();
    observer = std::make_unique<QtAccessibilityObserver>([weak](bool enabled) {
      if (const auto state = weak.lock(); state && state->active) {
        state->emitBoolean("qtScreenReaderChanged", enabled);
      }
    });
    QObject::connect(QGuiApplication::styleHints()->accessibility(),
        &QAccessibilityHints::contrastPreferenceChanged, observer.get(),
        [weak](Qt::ContrastPreference preference) {
          if (const auto state = weak.lock(); state && state->active) {
            state->emitBoolean("qtHighTextContrastChanged", preference == Qt::ContrastPreference::HighContrast);
          }
        });
  }

  void emitBoolean(const char* event, bool value) {
    requireOwner();
    if (!active) return;
    const std::weak_ptr<Impl> weak = shared_from_this();
    invoker->invokeAsync([weak, event, value](jsi::Runtime& runtime) {
      const auto state = weak.lock();
      if (!state || !state->active) return;
      state->requireOwner();
      auto emitter = runtime.global().getProperty(runtime, "__rctDeviceEventEmitter");
      if (!emitter.isObject()) return;
      auto object = emitter.asObject(runtime);
      object.getPropertyAsFunction(runtime, "emit").callWithThis(runtime, object,
          jsi::String::createFromAscii(runtime, event), value);
    });
  }

  void requireOwner() const noexcept {
    if (QThread::currentThread() != owner) std::terminate();
  }
  void requireActive(jsi::Runtime& runtime, size_t count, size_t expected) const {
    requireOwner();
    if (!active) throw jsi::JSError(runtime, "AccessibilityInfo: native observation is closed");
    if (count != expected) throw jsi::JSError(runtime, "AccessibilityInfo: invalid argument count");
  }
  void close() noexcept {
    if (!active) return;
    requireOwner();
    active = false;
    observer.reset();
  }
  fabric::ShadowViewMutationAdapter& adapter;
  const std::shared_ptr<react::CallInvoker> invoker;
  QThread* const owner;
  std::unique_ptr<QtAccessibilityObserver> observer;
  bool active{true};
};

QtAccessibilityInfoModule::QtAccessibilityInfoModule(std::shared_ptr<react::CallInvoker> invoker,
    fabric::ShadowViewMutationAdapter& adapter) : react::TurboModule(kModuleName, invoker) {
  if (!qGuiApp || QThread::currentThread() != qGuiApp->thread() || !invoker) {
    throw std::logic_error("AccessibilityInfo requires the Qt application owner and runtime invoker");
  }
  impl_ = std::make_shared<Impl>(std::move(invoker), adapter);
  impl_->start();
  methodMap_["isScreenReaderEnabled"] = MethodMetadata{0, isScreenReaderEnabled};
  methodMap_["isHighTextContrastEnabled"] = MethodMetadata{0, isHighTextContrastEnabled};
  methodMap_["setAccessibilityFocus"] = MethodMetadata{1, setAccessibilityFocus};
  methodMap_["announceForAccessibility"] = MethodMetadata{2, announceForAccessibility};
}

QtAccessibilityInfoModule::~QtAccessibilityInfoModule() { invalidate(); }
void QtAccessibilityInfoModule::invalidate() noexcept {
  if (impl_) impl_->close();
}

jsi::Value QtAccessibilityInfoModule::isScreenReaderEnabled(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value*, size_t count) {
  const auto state = static_cast<QtAccessibilityInfoModule&>(module).impl_;
  state->requireActive(runtime, count, 0);
  // Qt reports an active accessibility client, not whether that client speaks.
  return QAccessible::isActive();
}

jsi::Value QtAccessibilityInfoModule::isHighTextContrastEnabled(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value*, size_t count) {
  const auto state = static_cast<QtAccessibilityInfoModule&>(module).impl_;
  state->requireActive(runtime, count, 0);
  const auto preference = QGuiApplication::styleHints()->accessibility()->contrastPreference();
  return preference == Qt::ContrastPreference::HighContrast;
}

jsi::Value QtAccessibilityInfoModule::setAccessibilityFocus(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value* args, size_t count) {
  const auto state = static_cast<QtAccessibilityInfoModule&>(module).impl_;
  state->requireActive(runtime, count, 1);
  if (!args[0].isNumber() || !std::isfinite(args[0].getNumber()) || args[0].getNumber() <= 0 ||
      std::floor(args[0].getNumber()) != args[0].getNumber() ||
      args[0].getNumber() > std::numeric_limits<react::Tag>::max()) {
    throw jsi::JSError(runtime, "AccessibilityInfo: focus requires a positive mounted React tag");
  }
  const auto failure = state->adapter.sendAccessibilityEvent(static_cast<react::Tag>(args[0].getNumber()), "focus");
  if (failure.error != fabric::MountError::None) throw jsi::JSError(runtime, failure.detail.toStdString());
  return jsi::Value::undefined();
}

jsi::Value QtAccessibilityInfoModule::announceForAccessibility(jsi::Runtime& runtime,
    react::TurboModule& module, const jsi::Value* args, size_t count) {
  const auto state = static_cast<QtAccessibilityInfoModule&>(module).impl_;
  state->requireActive(runtime, count, 2);
  if (!args[0].isString() || !args[1].isBool()) {
    throw jsi::JSError(runtime, "AccessibilityInfo: announcement requires text and a boolean priority");
  }
  auto* window = state->adapter.hostWindow();
  if (!window) throw jsi::JSError(runtime, "AccessibilityInfo: announcement requires a live host window");
  const auto text = QString::fromStdString(args[0].asString(runtime).utf8(runtime));
  QAccessibleAnnouncementEvent event(window, text);
  event.setPoliteness(args[1].getBool() ? QAccessible::AnnouncementPoliteness::Assertive
                                      : QAccessible::AnnouncementPoliteness::Polite);
  QAccessible::updateAccessibility(&event);
  return jsi::Value::undefined();
}

}  // namespace rnqnx::runtime
