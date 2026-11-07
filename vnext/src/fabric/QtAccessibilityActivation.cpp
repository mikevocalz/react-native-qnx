#include "react-native-qnx/fabric/QtAccessibilityActivation.h"
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QMetaMethod>
#include <QThread>
#include <atomic>
#include <utility>
namespace rnqnx::fabric {
namespace { std::atomic<std::uint64_t> nextGeneration{1}; }
QtAccessibilityActivation::QtAccessibilityActivation(QQuickItem& item, AccessibilityActivationPolicy policy, Emit callback)
    : QObject(&item), item_(&item), policy_(policy), emit_(std::move(callback)),
      generation_(nextGeneration.fetch_add(1, std::memory_order_relaxed)) {}
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
  QObject::disconnect(connection_);
  connection_ = {};
}
void QtAccessibilityActivation::activate() {
  if (QThread::currentThread() != thread() || !live_ || !item_ ||
      !policy_.accessible || !policy_.registered || !policy_.hasHandler || policy_.disabled ||
      !item_->isEnabled() || !emit_) return;
  const auto callback = emit_;
  callback(); // Can synchronously destroy this View/receiver. No access after it.
}
}
