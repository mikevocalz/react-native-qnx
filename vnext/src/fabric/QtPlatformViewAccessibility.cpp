#include "react-native-qnx/fabric/QtViewAccessibility.h"
#include "react-native-qnx/fabric/QtAccessibilityActivation.h"
#include <QAccessibleObject>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <algorithm>

namespace rnqnx::fabric {
namespace {
constexpr auto marker = "_rnqnxPlatformAccessibleView";

QVariant attached(QQuickItem* item, const char* property) {
  if (!item || !qmlContext(item)) return {};
  return QQmlProperty::read(item, QString::fromLatin1(property), qmlContext(item));
}
bool ignored(QQuickItem* item) {
  const auto value = attached(item, "Accessible.ignored");
  return value.isValid() && value.toBool();
}
bool hiddenSubtree(QQuickItem* item) {
  for (auto* current = item; current; current = current->parentItem())
    if (current->property("rnqnxAccessibilityHideDescendants").toBool()) return true;
  return false;
}
bool visuallyHidden(QQuickItem* item) {
  if (!item || !item->window() || !item->window()->isVisible() || !item->isVisible() || item->boundingRect().isEmpty()) return true;
  for (auto* current = item; current; current = current->parentItem())
    if (qFuzzyIsNull(current->opacity())) return true;
  return false;
}
bool clippedAt(QQuickItem* item, int x, int y) {
  for (auto* current = item; current; current = current->parentItem())
    if (current->clip() && !current->contains(current->mapFromGlobal(QPointF(x, y)))) return true;
  return false;
}
QAccessibleInterface* exposed(QQuickItem* item) {
  if (!item || hiddenSubtree(item)) return nullptr;
  // Query first: reading an attached property can itself create Accessible.
  // Decorative native QML items must not acquire semantics by tree traversal.
  auto* interface = QAccessible::queryAccessibleInterface(item);
  return interface && !ignored(item) ? interface : nullptr;
}
QList<QAccessibleInterface*> children(QQuickItem* item, bool paintOrder = false) {
  QList<QAccessibleInterface*> result;
  if (!item || hiddenSubtree(item)) return result;
  auto items = item->childItems();
  if (paintOrder)
    std::stable_sort(items.begin(), items.end(), [](auto* left, auto* right) { return left->z() < right->z(); });
  for (auto* child : items) {
    if (auto* interface = exposed(child)) result.push_back(interface);
    else result.append(children(child, paintOrder)); // Keep semantic children of ignored layout wrappers.
  }
  return result;
}

// Only the renderer's explicitly marked generic Views use this interface.
// Native text, editable controls, windows and other Qt objects keep their own
// factories. All metadata remains the real public QML Accessible state; this
// adapter adds RN's registered named actions, which QML cannot express for
// showMenuAction (RN longpress).
class ViewInterface final : public QAccessibleObject, public QAccessibleActionInterface {
 public:
  explicit ViewInterface(QQuickItem* item) : QAccessibleObject(item) {}
  QQuickItem* item() const { return qobject_cast<QQuickItem*>(object()); }
  QWindow* window() const override { return item() ? item()->window() : nullptr; }
  QAccessibleInterface* parent() const override {
    for (auto* current = item() ? item()->parentItem() : nullptr; current; current = current->parentItem()) {
      if (auto* interface = exposed(current)) return interface;
    }
    return window() ? QAccessible::queryAccessibleInterface(window()) : nullptr;
  }
  int childCount() const override { return static_cast<int>(children(item()).size()); }
  QAccessibleInterface* child(int index) const override { return children(item()).value(index); }
  int indexOfChild(const QAccessibleInterface* value) const override {
    const auto list = children(item());
    for (qsizetype index = 0; index < list.size(); ++index)
      if (list[index] == value) return static_cast<int>(index);
    return -1;
  }
  QAccessibleInterface* childAt(int x, int y) const override {
    auto* currentItem = item();
    if (!currentItem || clippedAt(currentItem, x, y)) return nullptr;
    const auto list = children(item(), true);
    for (auto current = list.crbegin(); current != list.crend(); ++current) {
      if ((*current)->state().invisible) continue;
      if (auto* childItem = qobject_cast<QQuickItem*>((*current)->object()); childItem && clippedAt(childItem->parentItem(), x, y)) continue;
      if (auto* descendant = (*current)->childAt(x, y)) return descendant;
      if ((*current)->rect().contains(x, y)) return *current;
    }
    return nullptr;
  }
  QAccessibleInterface* focusChild() const override {
    auto* root = item();
    auto* focused = root && root->window() ? root->window()->activeFocusItem() : nullptr;
    if (!focused || (focused != root && !root->isAncestorOf(focused))) return nullptr;
    for (auto* current = focused; current; current = current->parentItem()) {
      if (auto* interface = exposed(current)) return interface;
      if (current == root) break;
    }
    return nullptr;
  }
  QRect rect() const override {
    auto* current = item();
    if (!current || !current->window()) return {};
    const auto scene = current->mapRectToScene(current->boundingRect());
    return QRectF(QPointF(current->window()->mapToGlobal(QPoint())) + scene.topLeft(), scene.size()).toAlignedRect();
  }
  QString text(QAccessible::Text type) const override {
    if (!item() || ignored(item()) || hiddenSubtree(item())) return {};
    if (type == QAccessible::Name) return attached(item(), "Accessible.name").toString();
    if (type == QAccessible::Description) return attached(item(), "Accessible.description").toString();
    if (type == QAccessible::Value) return item()->property("accessibilityValue").toString();
    if (type == QAccessible::Identifier) return attached(item(), "Accessible.id").toString();
    return {};
  }
  void setText(QAccessible::Text type, const QString& value) override {
    auto* current = item();
    if (!current || QThread::currentThread() != current->thread()) return;
    const char* name = type == QAccessible::Name ? "Accessible.name" :
        type == QAccessible::Description ? "Accessible.description" : nullptr;
    if (name) QQmlProperty::write(current, QString::fromLatin1(name), value, qmlContext(current));
  }
  QAccessible::Role role() const override {
    const auto value = attached(item(), "Accessible.role");
    return value.isValid() ? static_cast<QAccessible::Role>(value.toInt()) : QAccessible::Client;
  }
  QAccessible::State state() const override {
    QAccessible::State result;
    auto* current = item();
    if (!current) { result.invalid = true; return result; }
    result.invisible = ignored(current) || hiddenSubtree(current) || visuallyHidden(current);
    result.disabled = !current->isEnabled() || current->property("accessibilityStateDisabled").toBool();
    result.focusable = !result.disabled && (current->activeFocusOnTab() || attached(current, "Accessible.focusable").toBool());
    result.focused = current->hasActiveFocus();
    result.selected = attached(current, "Accessible.selected").toBool();
    result.selectable = attached(current, "Accessible.selectable").toBool();
    result.checkable = attached(current, "Accessible.checkable").toBool();
    result.checked = attached(current, "Accessible.checked").toBool();
    result.checkStateMixed = attached(current, "Accessible.checkStateMixed").toBool();
    result.pressed = attached(current, "Accessible.pressed").toBool();
    result.readOnly = attached(current, "Accessible.readOnly").toBool();
    result.busy = current->property("rnqnxAccessibilityBusy").toBool();
    const auto expanded = current->property("rnqnxAccessibilityExpanded");
    result.expandable = expanded.isValid();
    result.expanded = expanded.isValid() && expanded.toBool();
    result.collapsed = expanded.isValid() && !expanded.toBool();
    result.hasPopup = role() == QAccessible::ComboBox || actionNames().contains(QAccessibleActionInterface::showMenuAction());
    result.modal = current->property("rnqnxAccessibilityModal").toBool();
    if (current->window()) {
      const QRect bounds(current->window()->mapToGlobal(QPoint()), current->window()->size());
      result.offscreen = !bounds.intersects(rect());
    }
    return result;
  }
  void* interface_cast(QAccessible::InterfaceType type) override {
    return type == QAccessible::ActionInterface ? static_cast<QAccessibleActionInterface*>(this) : nullptr;
  }
  QStringList actionNames() const override {
    auto* current = item();
    if (!current || ignored(current) || hiddenSubtree(current) || visuallyHidden(current) || !current->isEnabled() ||
        current->property("accessibilityStateDisabled").toBool()) return {};
    auto* receiver = current->findChild<QtAccessibilityActivation*>(QString(), Qt::FindDirectChildrenOnly);
    auto result = receiver ? receiver->actionNames() : QStringList();
    if (current->activeFocusOnTab() || attached(current, "Accessible.focusable").toBool())
      result.push_back(QAccessibleActionInterface::setFocusAction());
    return result;
  }
  void doAction(const QString& name) override {
    auto* current = item();
    if (!current || QThread::currentThread() != current->thread() || !actionNames().contains(name)) return;
    if (name == QAccessibleActionInterface::setFocusAction()) {
      current->forceActiveFocus(Qt::OtherFocusReason);
      return;
    }
    if (auto* receiver = current->findChild<QtAccessibilityActivation*>(QString(), Qt::FindDirectChildrenOnly))
      receiver->performAction(name);
  }
  QStringList keyBindingsForAction(const QString&) const override { return {}; }
};

QAccessibleInterface* factory(const QString&, QObject* object) {
  auto* item = qobject_cast<QQuickItem*>(object);
  if (!item || !item->property(marker).toBool()) return nullptr;
  return new ViewInterface(item);
}
}

void preparePlatformViewAccessibility(QQuickItem& item) {
  Q_ASSERT(QThread::currentThread() == item.thread());
  static const bool installed = [] { QAccessible::installFactory(factory); return true; }();
  Q_UNUSED(installed);
  item.setProperty(marker, true);
  setPlatformViewAccessibilityIgnored(item, true);
}
bool setPlatformViewAccessibilityIgnored(QQuickItem& item, bool ignoredValue) {
  // A native Qt window recursively flattens ignored wrappers. An explicitly
  // hidden root must remain a traversal boundary with invisible state and no
  // children, otherwise native Text descendants escape the hidden subtree.
  // Only this renderer-owned generic View is changed. Native descendants keep
  // their metadata and QML bindings intact.
  const bool boundary = item.property("rnqnxAccessibilityHideDescendants").toBool();
  return QQmlProperty::write(&item, QStringLiteral("Accessible.ignored"),
      boundary ? false : ignoredValue, qmlContext(&item));
}
bool platformViewAccessibilityHidden(QQuickItem& item) {
  return hiddenSubtree(&item) || visuallyHidden(&item) || ignored(&item);
}
void notifyPlatformAccessibilityTreeChanged(QQuickItem& item) {
  QObject* target = item.window();
  for (auto* parent = item.parentItem(); parent; parent = parent->parentItem()) {
    if (auto* interface = exposed(parent)) { target = interface->object(); break; }
  }
  if (target) {
    QAccessibleEvent event(target, QAccessible::ObjectReorder);
    QAccessible::updateAccessibility(&event);
  }
}
} // namespace rnqnx::fabric

