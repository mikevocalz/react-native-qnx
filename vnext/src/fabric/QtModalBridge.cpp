#include "react-native-qnx/fabric/QtModalBridge.h"

#include <utility>

namespace rnqnx::fabric {

QString QtModalBridge::modalObjectName() {
  return QStringLiteral("rnqnxModal");
}

QtModalBridge::QtModalBridge(QQuickItem& modal, std::function<void()> onRequestClose)
    : QObject(&modal), modal_(modal), onRequestClose_(std::move(onRequestClose)) {
  connect(&modal_, SIGNAL(requestClose()), this, SLOT(requestClose()));
  connect(&modal_, &QQuickItem::windowChanged, this, &QtModalBridge::windowChanged);
  if (modal_.window() != nullptr) {
    enter(*modal_.window());
  }
}

bool QtModalBridge::insideAnyModal(const QQuickItem* item) {
  for (; item != nullptr; item = item->parentItem()) {
    if (item->objectName() == modalObjectName()) {
      return true;
    }
  }
  return false;
}

bool QtModalBridge::insideOtherModal(const QQuickItem* item) const {
  for (; item != nullptr && item != &modal_; item = item->parentItem()) {
    if (item->objectName() == modalObjectName()) {
      return true;
    }
  }
  return false;
}

void QtModalBridge::windowChanged(QQuickWindow* window) {
  leave();
  if (window != nullptr) {
    enter(*window);
  }
}

void QtModalBridge::enter(QQuickWindow& window) {
  window_ = &window;
  previousFocus_ = window.activeFocusItem();
  modal_.forceActiveFocus();
  connect(&window, &QQuickWindow::activeFocusItemChanged, this, &QtModalBridge::activeFocusChanged);
}

void QtModalBridge::leave() {
  if (window_ == nullptr) {
    return;
  }
  QQuickWindow* window = window_;
  window_.clear();
  disconnect(window, &QQuickWindow::activeFocusItemChanged, this, &QtModalBridge::activeFocusChanged);
  // A modal opened above this one owns the keyboard now; leave focus alone.
  if (insideOtherModal(window->activeFocusItem())) {
    return;
  }
  if (previousFocus_ != nullptr && previousFocus_->window() == window) {
    previousFocus_->forceActiveFocus();
  }
  previousFocus_.clear();
}

void QtModalBridge::activeFocusChanged() {
  if (reclaiming_ || window_ == nullptr) {
    return;
  }
  QQuickItem* focused = window_->activeFocusItem();
  if (focused == nullptr || insideAnyModal(focused)) {
    return;
  }
  reclaiming_ = true;
  modal_.forceActiveFocus();
  reclaiming_ = false;
}

void QtModalBridge::requestClose() {
  if (onRequestClose_) {
    onRequestClose_();
  }
}

}  // namespace rnqnx::fabric
