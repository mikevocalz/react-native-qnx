#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/fabric/QtViewAccessibility.h"

#include <QAccessible>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <algorithm>

namespace rnqnx::fabric {

QQuickWindow* ShadowViewMutationAdapter::hostWindow() const {
  return layer_.rootItem().window();
}

MutationFailure ShadowViewMutationAdapter::sendAccessibilityEvent(
    facebook::react::Tag tag, const std::string& eventType) {
  const auto failure = [tag](const char* reason) {
    return MutationFailure{facebook::react::ShadowViewMutation::Type::Update,
        tag, MountError::UnsupportedAccessibility, QString::fromLatin1(reason)};
  };
  auto* item = layer_.itemForTag(tag);
  if (!item) return failure("AccessibilityInfo: focus target is no longer mounted");
  if (QThread::currentThread() != item->thread()) {
    return failure("AccessibilityInfo: focus requires the Qt owner thread");
  }
  if (eventType != "focus") return failure("AccessibilityInfo: only the focus event has a Qt provider");
  if (!item->window() || !item->parentItem() || !item->isVisible() || !item->isEnabled() ||
      item->property("accessibilityStateDisabled").toBool()) {
    return failure("AccessibilityInfo: focus target must be mounted, visible and enabled");
  }
  const auto* accessible = QAccessible::queryAccessibleInterface(item);
  if (!accessible || !accessible->isValid() || accessible->state().invisible || platformViewAccessibilityHidden(*item)) {
    return failure("AccessibilityInfo: focus target has no exposed Qt accessibility interface");
  }
  const QPointer<QQuickItem> target(item);
  target->forceActiveFocus(Qt::OtherFocusReason);
  if (!target) return failure("AccessibilityInfo: focus target was destroyed during focus delivery");
  QAccessibleEvent event(target, QAccessible::Focus);
  QAccessible::updateAccessibility(&event);
  return {};
}

bool ShadowViewMutationAdapter::requestModalClose() {
  // Follow visual stacking rather than QHash iteration or React tag allocation.
  const std::function<bool(QQuickItem*)> visit = [&](QQuickItem* item) {
    if (!item->isVisible() || item->window() != hostWindow()) return false;
    auto children = item->childItems();
    std::stable_sort(children.begin(), children.end(), [](const auto* a, const auto* b) { return a->z() < b->z(); });
    for (auto child = children.crbegin(); child != children.crend(); ++child) {
      if (visit(*child)) return true;
    }
    for (auto callback = modalCloseCallbacks_.cbegin(); callback != modalCloseCallbacks_.cend(); ++callback) {
      if (layer_.itemForTag(callback.key()) == item) {
        const auto dispatch = callback.value();
        dispatch();
        return true;
      }
    }
    return false;
  };
  return visit(&layer_.rootItem());
}

}  // namespace rnqnx::fabric
