#include "react-native-qnx/fabric/QtSwitchBridge.h"

namespace rnqnx::fabric {

QtSwitchBridge::QtSwitchBridge(
    std::function<void(bool)> onChange,
    QQuickItem* item,
    bool initialValue,
    QObject* parent)
    : QObject(parent),
      onChange_(std::move(onChange)),
      item_(item),
      lastValue_(initialValue) {}

void QtSwitchBridge::setExpectedValue(bool value) {
  lastValue_ = value;
}

void QtSwitchBridge::setValue(bool value) {
  lastValue_ = value;
  if (item_) item_->setProperty("value", value);
}

void QtSwitchBridge::setOnChange(std::function<void(bool)> onChange) {
  onChange_ = std::move(onChange);
}

void QtSwitchBridge::notify() {
  if (!item_) return;
  const bool value = item_->property("value").toBool();
  if (value == lastValue_) {
    return;
  }
  lastValue_ = value;
  // The callback can synchronously reconcile or destroy the item/bridge.
  const auto onChange = onChange_;
  if (onChange) onChange(value);
}

}  // namespace rnqnx::fabric
