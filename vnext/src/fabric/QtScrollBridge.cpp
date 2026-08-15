#include "react-native-qnx/fabric/QtScrollBridge.h"

#include <utility>

namespace rnqnx::fabric {

QtScrollBridge::QtScrollBridge(std::function<void()> onSignal, QObject* parent)
    : QObject(parent), onSignal_(std::move(onSignal)) {}

void QtScrollBridge::notify() {
  if (onSignal_) {
    onSignal_();
  }
}

}  // namespace rnqnx::fabric
