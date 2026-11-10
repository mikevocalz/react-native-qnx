#include "react-native-qnx/fabric/QtAccessibilityActivation.h"
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QMetaMethod>
#include <QThread>
#include <atomic>
#include <utility>
#if RNQNX_ENABLE_PLATFORM_SERVICES
#include <QAccessible>
#include "react-native-qnx/fabric/QtViewAccessibility.h"
#endif
namespace rnqnx::fabric {
namespace { std::atomic<std::uint64_t> nextGeneration{1}; }
QtAccessibilityActivation::QtAccessibilityActivation(QQuickItem& item, AccessibilityActivationPolicy policy, Emit callback)
    : QObject(&item), item_(&item), policy_(policy), emit_(std::move(callback)),
      generation_(nextGeneration.fetch_add(1, std::memory_order_relaxed)) {}
#if RNQNX_ENABLE_PLATFORM_SERVICES
QtAccessibilityActivation::QtAccessibilityActivation(QQuickItem& item, AccessibilityActivationPolicy policy,
    QStringList actions, NamedEmit callback)
    : QObject(&item), item_(&item), policy_(policy), actions_(std::move(actions)), namedEmit_(std::move(callback)),
      generation_(nextGeneration.fetch_add(1, std::memory_order_relaxed)) {}
QStringList QtAccessibilityActivation::actionNames() const {
  if (!live_ || !item_ || !policy_.accessible || !policy_.registered || !policy_.hasHandler ||
      policy_.disabled || !item_->isEnabled() || platformViewAccessibilityHidden(*item_) || !namedEmit_) return {};
  QStringList result;
  for (const auto& action : actions_) {
    if (action == QStringLiteral("activate")) result.push_back(QAccessibleActionInterface::pressAction());
    else if (action == QStringLiteral("longpress")) result.push_back(QAccessibleActionInterface::showMenuAction());
    else if (action == QStringLiteral("increment")) result.push_back(QAccessibleActionInterface::increaseAction());
    else if (action == QStringLiteral("decrement")) result.push_back(QAccessibleActionInterface::decreaseAction());
  }
  return result;
}
void QtAccessibilityActivation::performAction(const QString& action) {
  if (QThread::currentThread() != thread() || !actionNames().contains(action)) return;
  std::string name;
  if (action == QAccessibleActionInterface::pressAction()) name = "activate";
  else if (action == QAccessibleActionInterface::showMenuAction()) name = "longpress";
  else if (action == QAccessibleActionInterface::increaseAction()) name = "increment";
  else if (action == QAccessibleActionInterface::decreaseAction()) name = "decrement";
  else return;
  const auto callback = namedEmit_;
  callback(name); // The emitter may synchronously remove this receiver.
}
#endif
bool QtAccessibilityActivation::connectPress(QString& error) {
  error.clear();
  if (!live_ || !item_ || QThread::currentThread() != thread()) {
    error = QStringLiteral("Accessibility activation requires its live Qt owner");
    return false;
  }
  // A replacement props generation owns a replacement receiver. Do not admit
  // repeated connections or let a disconnected generation be rearmed.
  if (connection_) {
    error = QStringLiteral("Accessibility activation already connected");
    return false;
  }
  QQmlProperty signal(item_, QStringLiteral("Accessible.onPressAction"), qmlContext(item_));
  const auto method = signal.method();
  const auto slot = metaObject()->method(metaObject()->indexOfSlot("activate()"));
  if (!signal.isSignalProperty() || !signal.object() || !method.isValid() || !slot.isValid()) {
    error = QStringLiteral("Qt Accessible press signal unavailable");
    return false;
  }
  connection_ = QObject::connect(signal.object(), method, this, slot, Qt::DirectConnection);
  if (!connection_) {
    error = QStringLiteral("Qt Accessible press connection failed");
    return false;
  }
  return true;
}
void QtAccessibilityActivation::invalidate() noexcept {
  live_ = false;
  emit_ = {};
#if RNQNX_ENABLE_PLATFORM_SERVICES
  namedEmit_ = {};
  actions_.clear();
#endif
  QObject::disconnect(connection_);
  connection_ = {};
}
void QtAccessibilityActivation::activate() {
#if RNQNX_ENABLE_PLATFORM_SERVICES
  if (namedEmit_) {
    performAction(QAccessibleActionInterface::pressAction());
    return;
  }
#endif
  if (QThread::currentThread() != thread() || !live_ || !item_ ||
      !policy_.accessible || !policy_.registered || !policy_.hasHandler || policy_.disabled ||
      !item_->isEnabled() || !emit_) return;
  const auto callback = emit_;
  callback(); // Can synchronously destroy this View/receiver. No access after it.
}
}
