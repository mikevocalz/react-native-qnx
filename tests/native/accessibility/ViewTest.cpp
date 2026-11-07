// Native Qt semantic-tree checks. No OS accessibility consumer is exercised.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtViewAccessibility.h"
#if RNQNX_ACCESSIBILITY_MOUNTING_TEST
#include "react-native-qnx/views/QtQuickItemFactory.h"
#endif
#include <QAccessible>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace rnqnx::fabric;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
#if RNQNX_ACCESSIBILITY_MOUNTING_TEST
void checkMountingPath(QQmlEngine& engine) {
  QQuickWindow window;
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray("import QtQuick\nItem { property color backgroundColor: 'transparent' }"));
  factory.registerType(QStringLiteral("TextInput"), QByteArray("import QtQuick\nItem { property color backgroundColor: 'transparent' }"));
  QtQuickMountingLayer layer(factory, *window.contentItem(), 100);
  require(layer.create(101, QStringLiteral("View")) == MountError::None &&
          layer.insert(100, 101, 0) == MountError::None, "mounted generic View unavailable");
  ViewStyle style;
  style.accessible = true;
  style.accessibilityLabel = QStringLiteral("Mounted fixture");
  require(layer.updateStyle(101, style) == MountError::None, "mounted View semantics failed");
  auto* item = layer.itemForTag(101);
  auto* iface = QAccessible::queryAccessibleInterface(item);
  require(iface && iface->text(QAccessible::Name) == style.accessibilityLabel, "mount path omitted semantic mapping");
  style.accessible = false;
  require(layer.updateStyle(101, style) == MountError::None && iface->text(QAccessible::Name).isEmpty() &&
          item->property("accessibilityLabel").toString().isEmpty(), "mount path retained metadata");
  // The same ViewStyle application on another component is deliberately excluded.
  require(layer.create(102, QStringLiteral("TextInput")) == MountError::None, "excluded component unavailable");
  style.accessible = true;
  require(layer.updateStyle(102, style) == MountError::None &&
          !QAccessible::queryAccessibleInterface(layer.itemForTag(102)), "non-View received generic semantics");
  const auto id = QAccessible::uniqueId(iface);
  require(layer.destroy(101) == MountError::None && !QAccessible::accessibleInterface(id), "mount deletion retained interface");
}
#endif
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    QQmlEngine engine;
#if RNQNX_ACCESSIBILITY_MOUNTING_TEST
    checkMountingPath(engine);
#endif
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nItem {}", QUrl());
    require(component.isReady(), "generic View QML unavailable");
    QQuickWindow window;
    window.resize(320, 240);
    std::unique_ptr<QObject> parentObject(component.create());
    std::unique_ptr<QObject> childObject(component.create());
    std::unique_ptr<QObject> siblingObject(component.create());
    auto* parent = qobject_cast<QQuickItem*>(parentObject.get());
    auto* child = qobject_cast<QQuickItem*>(childObject.get());
    auto* sibling = qobject_cast<QQuickItem*>(siblingObject.get());
    require(parent && child && sibling, "generic Views unavailable");
    parent->setParentItem(window.contentItem()); parent->setSize(QSizeF(300, 200));
    child->setParentItem(parent); child->setSize(QSizeF(100, 40));
    sibling->setParentItem(parent); sibling->setSize(QSizeF(100, 40)); sibling->setY(50);
    ViewStyle group;
    group.accessible = true;
    group.accessibilityLabel = QStringLiteral("Fixture group");
    ViewStyle style;
    style.accessible = true;
    style.accessibilityRole = QStringLiteral("checkbox");
    style.accessibilityLabel = QStringLiteral("Fixture choice");
    style.accessibilityHint = QStringLiteral("Fixture description");
    style.accessibilityStateSelected = true;
    style.accessibilityStateCheckable = true;
    style.accessibilityStateMixed = true;
    QString error;
    require(applyViewAccessibility(*parent, group, error), "parent semantics failed");
    require(applyViewAccessibility(*child, style, error), "child semantics failed");
    require(applyViewAccessibility(*sibling, group, error), "sibling semantics failed");
    window.show();
    QCoreApplication::processEvents();
    auto* tree = QAccessible::queryAccessibleInterface(parent);
    auto* iface = QAccessible::queryAccessibleInterface(child);
    require(tree && iface, "Qt semantic interfaces unavailable");
    require(tree->childCount() == 2 && tree->child(0)->object() == child && tree->child(1)->object() == sibling,
            "semantic child tree/order mismatch");
    require(iface->parent()->object() == parent && iface->role() == QAccessible::CheckBox &&
            iface->text(QAccessible::Name) == style.accessibilityLabel &&
            iface->text(QAccessible::Description) == style.accessibilityHint, "semantic identity mismatch");
    require(iface->state().selected && iface->state().checkable && iface->state().checkStateMixed &&
            !iface->state().checked, "semantic mixed/selected state mismatch");
    // Disabled derives from the real item; metadata must not change input enablement.
    child->setEnabled(false);
    require(iface->state().disabled, "native disabled state missing");
    child->setEnabled(true);
    style.accessibilityStateDisabled = true;
    style.accessibilityRole = QStringLiteral("text");
    style.accessibilityLabel = QStringLiteral("Updated fixture");
    style.accessibilityHint.clear();
    style.accessibilityStateSelected = false;
    style.accessibilityStateMixed = false;
    style.accessibilityStateChecked = true;
    require(applyViewAccessibility(*child, style, error), "semantic update failed");
    require(child->isEnabled() && iface->role() == QAccessible::StaticText &&
            iface->text(QAccessible::Name) == style.accessibilityLabel &&
            iface->text(QAccessible::Description).isEmpty() && !iface->state().selected &&
            !iface->state().checkStateMixed && iface->state().checked, "semantic update was stale");
    style.accessible = false;
    require(applyViewAccessibility(*child, style, error), "semantic clearing failed");
    require(tree->childCount() == 1 && iface->text(QAccessible::Name).isEmpty() &&
            iface->text(QAccessible::Description).isEmpty() && !iface->state().selected &&
            !iface->state().checked && !iface->state().checkable, "inaccessible View retained semantic metadata");
    style.accessible = true;
    require(applyViewAccessibility(*child, style, error) && !iface->text(QAccessible::Name).isEmpty(), "semantic re-exposure failed");
    style.accessibilityRole = QStringLiteral("unsupported-fixture-role");
    require(!applyViewAccessibility(*child, style, error) && error == QStringLiteral("Unsupported accessibility View role") &&
            tree->childCount() == 1 && iface->text(QAccessible::Name).isEmpty(), "unsupported role exposed stale semantics");
    style.accessibilityRole = QStringLiteral("none");
    require(applyViewAccessibility(*child, style, error) && tree->childCount() == 2 && iface->role() == QAccessible::Client,
            "role none incorrectly hid an accessible View");
    const auto id = QAccessible::uniqueId(iface);
    QPointer<QQuickItem> previous(child);
    childObject.reset();
    require(previous.isNull() && QAccessible::accessibleInterface(id) == nullptr && tree->childCount() == 1,
            "deleted View retained semantic interface");
    // Non-QML objects fail explicitly instead of silently producing a capability pass.
    QQuickItem noContext;
    require(!applyViewAccessibility(noContext, style, error) && !error.isEmpty(), "non-QML target silently accepted");
    std::cout << "RNQNX_ACCESSIBILITY_VIEW_OK tree identity state update clear reject delete\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_ACCESSIBILITY_VIEW_FAIL " << error.what() << '\n'; return 1;
  }
}
