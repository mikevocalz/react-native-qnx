#include "react-native-qnx/fabric/QtModalBridge.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace rnqnx::fabric {

namespace {
// Non-owning registration, used only on the Qt GUI owner thread. Filtering by
// window keeps independent surfaces separate; QPointers tolerate item teardown.
std::vector<QPointer<QtModalBridge>>& activeModals() {
  static std::vector<QPointer<QtModalBridge>> bridges;
  return bridges;
}
}  // namespace

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

QtModalBridge::~QtModalBridge() {
  leave();
}

QtModalBridge* QtModalBridge::topModal(QQuickWindow* window) {
  const auto& bridges = activeModals();
  for (auto it = bridges.rbegin(); it != bridges.rend(); ++it) {
    if (*it != nullptr && (*it)->window_ == window) {
      return it->data();
    }
  }
  return nullptr;
}

bool QtModalBridge::contains(const QQuickItem* item) const {
  for (; item != nullptr; item = item->parentItem()) {
    if (item == &modal_) {
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
  // Register before changing focus so lower modals cannot reclaim it.
  activeModals().emplace_back(this);
  connect(&window, &QQuickWindow::activeFocusItemChanged, this, &QtModalBridge::activeFocusChanged);
  modal_.forceActiveFocus();
}

void QtModalBridge::leave() {
  QQuickWindow* window = window_;
  const bool wasTop = window != nullptr && topModal(window) == this;
  auto previousFocus = previousFocus_;
  auto& bridges = activeModals();
  // If a covered modal closes first, its successor must inherit the original
  // restoration target rather than retain a pointer to a disappearing item.
  if (window != nullptr) {
    for (const auto& bridge : bridges) {
      if (bridge != nullptr && bridge != this && bridge->window_ == window &&
          contains(bridge->previousFocus_)) {
        bridge->previousFocus_ = previousFocus;
      }
    }
  }
  std::erase_if(bridges, [this](const auto& bridge) { return bridge == nullptr || bridge == this; });
  window_.clear();
  previousFocus_.clear();
  if (window == nullptr) {
    return;
  }
  disconnect(window, &QQuickWindow::activeFocusItemChanged, this, &QtModalBridge::activeFocusChanged);
  if (!wasTop) {
    return;
  }
  auto* next = topModal(window);
  if (previousFocus != nullptr && previousFocus->window() == window &&
      (next == nullptr || next->contains(previousFocus))) {
    previousFocus->forceActiveFocus();
  } else if (next != nullptr) {
    next->modal_.forceActiveFocus();
  }
}

void QtModalBridge::activeFocusChanged() {
  if (reclaiming_ || window_ == nullptr || topModal(window_) != this) {
    return;
  }
  QQuickItem* focused = window_->activeFocusItem();
  if (focused == nullptr || contains(focused)) {
    return;
  }
  reclaiming_ = true;
  modal_.forceActiveFocus();
  reclaiming_ = false;
}

void QtModalBridge::requestClose() {
  if (window_ != nullptr && topModal(window_) == this && onRequestClose_) {
    onRequestClose_();
  }
}

}  // namespace rnqnx::fabric
