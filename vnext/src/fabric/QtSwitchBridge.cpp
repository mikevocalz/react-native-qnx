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

void QtSwitchBridge::notify() {
  const bool value = item_->property("value").toBool();
  if (value == lastValue_) {
    return;
  }
  lastValue_ = value;
  onChange_(value);
}

}  // namespace rnqnx::fabric
