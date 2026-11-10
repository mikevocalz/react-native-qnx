#include "react-native-qnx/fabric/QtAccessibilityActivation.h"
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtViewAccessibility.h"
#include <QAccessible>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace rnqnx::fabric;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::unique_ptr<QQuickItem> create(QQmlEngine& engine, const QByteArray& source) {
  QQmlComponent component(&engine);
  component.setData(source, QUrl());
  require(component.isReady(), "QML fixture did not compile");
  auto* item = qobject_cast<QQuickItem*>(component.create());
  require(item, "QML fixture did not create an item");
  return std::unique_ptr<QQuickItem>(item);
}
void apply(QQuickItem& item, const ViewStyle& style) {
  QString error;
  require(applyViewAccessibility(item, style, error), error.toUtf8().constData());
}
bool contains(QAccessibleInterface* root, const QObject* object) {
  if (!root) return false;
  if (root->object() == object) return true;
  for (int index = 0; index < root->childCount(); ++index)
    if (contains(root->child(index), object)) return true;
  return false;
}
void contract() {
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(400, 300);
  auto parent = create(engine, "import QtQuick\nItem {}");
  auto wrapper = create(engine, "import QtQuick\nItem {}");
  auto child = create(engine, "import QtQuick\nItem {}");
  auto text = create(engine, "import QtQuick\nText { text: 'Native Qt text'; Accessible.role: Accessible.StaticText; Accessible.name: text }");
  auto decoration = create(engine, "import QtQuick\nRectangle { width: 20; height: 20 }");
  for (auto* item : {parent.get(), wrapper.get(), child.get()}) preparePlatformViewAccessibility(*item);
  parent->setParentItem(window.contentItem()); parent->setSize(QSizeF(300, 240));
  wrapper->setParentItem(parent.get()); wrapper->setSize(QSizeF(200, 180));
  child->setParentItem(wrapper.get()); child->setSize(QSizeF(100, 50)); child->setPosition(QPointF(20, 30));
  text->setParentItem(parent.get()); text->setY(100);
  decoration->setParentItem(parent.get());
  ViewStyle group; group.accessible = true; group.accessibilityLabel = QStringLiteral("Group");
  ViewStyle style; style.accessible = true; style.accessibilityLabel = QStringLiteral("Control");
  style.accessibilityRole = QStringLiteral("button");
  style.accessibilityHint = QStringLiteral("Hint");
  style.accessibilityStateCheckable = true;
  style.accessibilityStateMixed = true;
  style.accessibilityStateSelected = true;
  style.accessibilityStateBusy = true;
  style.accessibilityStateExpanded = false;
  apply(*parent, group); apply(*wrapper, ViewStyle()); apply(*child, style);
  window.show(); QCoreApplication::processEvents();
  auto* tree = QAccessible::queryAccessibleInterface(parent.get());
  auto* interface = QAccessible::queryAccessibleInterface(child.get());
  auto* nativeText = QAccessible::queryAccessibleInterface(text.get());
  require(tree, "Registered parent interface missing");
  require(interface, "Registered child interface missing");
  require(nativeText, "Native Qt text interface missing");
  require(nativeText->role() == QAccessible::StaticText && nativeText->text(QAccessible::Name) == QStringLiteral("Native Qt text"),
      "Scoped factory replaced native Qt text semantics");
  require(tree->childCount() == 2 && tree->child(0) == interface && tree->child(1) == nativeText &&
      tree->indexOfChild(interface) == 0 && interface->parent() == tree, "Ignored wrapper broke semantic parent/order");
  require(interface->rect().topLeft() == child->mapToGlobal(QPointF()).toPoint() &&
      tree->childAt(interface->rect().center().x(), interface->rect().center().y()) == interface,
      "Global bounds or semantic hit test mismatch");
  require(QAccessible::queryAccessibleInterface(decoration.get()) == nullptr, "Tree query made decorative native QML accessible");
  require(interface->text(QAccessible::Name) == style.accessibilityLabel &&
      interface->text(QAccessible::Description) == style.accessibilityHint &&
      interface->state().selected && interface->state().checkable && interface->state().checkStateMixed &&
      interface->state().busy && interface->state().expandable && interface->state().collapsed,
      "Registered View state not observable");
  ViewStyle hidden; hidden.accessibilityHideDescendants = true;
  apply(*wrapper, hidden);
  require(tree->childCount() == 1 && interface->state().invisible && interface->text(QAccessible::Name).isEmpty(),
      "Accessibility-hidden subtree exposed a semantic descendant");
  apply(*wrapper, ViewStyle());
  require(tree->childCount() == 2 && !interface->state().invisible,
      "Accessibility-hidden subtree failed to restore descendants");
  {
    auto overlap = create(engine, "import QtQuick\nItem {}");
    preparePlatformViewAccessibility(*overlap);
    overlap->setParentItem(parent.get()); overlap->setPosition(QPointF(20, 30)); overlap->setSize(QSizeF(200, 50));
    overlap->setZ(5); wrapper->setZ(10); apply(*overlap, group);
    const auto center = interface->rect().center();
    require(tree->childAt(center.x(), center.y()) == interface, "Accessibility hit testing ignored visual z order");
    wrapper->setClip(true); wrapper->setSize(QSizeF(30, 30));
    const auto* behindClip = tree->childAt(center.x(), center.y());
    require(behindClip && behindClip->object() == overlap.get(), "Ignored clipping wrapper leaked a hit-test descendant");
    wrapper->setClip(false); wrapper->setSize(QSizeF(200, 180)); wrapper->setZ(0);
  }
  {
    auto layout = create(engine, "import QtQuick\nItem {}");
    auto hiddenRoot = create(engine, "import QtQuick\nItem {}");
    auto native = create(engine, "import QtQuick\nText { property bool modelIgnored: false; text: 'Hidden native text'; Accessible.role: Accessible.StaticText; Accessible.name: text; Accessible.ignored: modelIgnored }");
    preparePlatformViewAccessibility(*layout); preparePlatformViewAccessibility(*hiddenRoot);
    layout->setParentItem(window.contentItem()); layout->setSize(QSizeF(200, 180));
    hiddenRoot->setParentItem(layout.get()); hiddenRoot->setSize(QSizeF(180, 150));
    native->setParentItem(hiddenRoot.get());
    apply(*layout, ViewStyle()); apply(*hiddenRoot, ViewStyle());
    auto* windowTree = QAccessible::queryAccessibleInterface(&window);
    require(contains(windowTree, native.get()), "Window-root negative control did not expose native text");
    apply(*hiddenRoot, hidden);
    require(!contains(windowTree, native.get()) && platformViewAccessibilityHidden(*native), "Window traversed through hidden ignored wrappers");
    auto* boundary = QAccessible::queryAccessibleInterface(hiddenRoot.get());
    require(boundary && boundary->state().invisible && boundary->childCount() == 0, "Hidden root was not an invisible traversal boundary");
    native->setProperty("modelIgnored", true);
    native->setProperty("modelIgnored", false);
    require(!contains(windowTree, native.get()), "Native ignored-property change escaped a hidden ancestor");
    auto inserted = create(engine, "import QtQuick\nText { text: 'Inserted native text'; Accessible.role: Accessible.StaticText; Accessible.name: text }");
    inserted->setParentItem(hiddenRoot.get());
    require(!contains(windowTree, inserted.get()), "New native child escaped a hidden subtree");
    inserted->setParentItem(window.contentItem());
    require(contains(windowTree, inserted.get()), "Reparented native child retained inherited suppression");
    inserted->setParentItem(hiddenRoot.get());
    require(!contains(windowTree, inserted.get()), "Reparented native child escaped inherited suppression");
    apply(*hiddenRoot, ViewStyle());
    require(contains(windowTree, native.get()) && contains(windowTree, inserted.get()), "Unhidden native descendants did not restore intended exposure");
    native->setProperty("modelIgnored", true);
    require(!contains(windowTree, native.get()), "Hidden-root update removed the native ignored-property binding");
    native->setProperty("modelIgnored", false);
    require(contains(windowTree, native.get()), "Native ignored-property binding no longer updates after unhide");
    apply(*hiddenRoot, hidden);
    inserted.reset(); // No observer or cached interface may survive its QObject.
  }
  const auto identity = QAccessible::uniqueId(interface);
  const std::pair<const char*, QAccessible::Role> roles[] = {
    {"combobox", QAccessible::ComboBox}, {"menu", QAccessible::PopupMenu},
    {"menuitem", QAccessible::MenuItem}, {"dialog", QAccessible::Dialog},
    {"list", QAccessible::List}, {"listbox", QAccessible::List}, {"option", QAccessible::ListItem},
    {"slider", QAccessible::Slider}, {"adjustable", QAccessible::Slider},
    {"progressbar", QAccessible::ProgressBar}, {"separator", QAccessible::Separator},
    {"tab", QAccessible::PageTab}, {"tablist", QAccessible::PageTabList},
    {"toolbar", QAccessible::ToolBar}, {"tooltip", QAccessible::ToolTip},
    {"group", QAccessible::Grouping}, {"tree", QAccessible::Tree}, {"treeitem", QAccessible::TreeItem},
    {"table", QAccessible::Table}, {"row", QAccessible::Row}, {"cell", QAccessible::Cell},
    {"columnheader", QAccessible::ColumnHeader}, {"rowheader", QAccessible::RowHeader},
  };
  for (const auto& [name, expected] : roles) {
    style.accessibilityRole = QString::fromLatin1(name); apply(*child, style);
    require(interface->role() == expected && QAccessible::uniqueId(interface) == identity,
        "Role mapping replaced identity or reported a different role");
  }
  QStringList calls;
  auto* receiver = new QtAccessibilityActivation(*child, {true, true, true, false},
      {QStringLiteral("activate"), QStringLiteral("longpress"), QStringLiteral("increment"), QStringLiteral("decrement")},
      [&](const std::string& name) { calls.push_back(QString::fromStdString(name)); });
  auto* actions = interface->actionInterface();
  require(actions, "Registered action interface missing");
  auto registered = actions->actionNames();
  registered.removeAll(QAccessibleActionInterface::setFocusAction());
  require(registered == QStringList({QAccessibleActionInterface::pressAction(), QAccessibleActionInterface::showMenuAction(),
      QAccessibleActionInterface::increaseAction(), QAccessibleActionInterface::decreaseAction()}),
      "Qt action list is not the registered RN list plus native focus");
  actions->doAction(QAccessibleActionInterface::pressAction());
  actions->doAction(QAccessibleActionInterface::showMenuAction());
  actions->doAction(QAccessibleActionInterface::increaseAction());
  actions->doAction(QAccessibleActionInterface::decreaseAction());
  actions->doAction(QStringLiteral("unknown"));
  require(calls == QStringList({QStringLiteral("activate"), QStringLiteral("longpress"), QStringLiteral("increment"), QStringLiteral("decrement")}),
      "Named Qt actions did not dispatch exactly once");
  parent->setOpacity(0);
  require(interface->state().invisible && actions->actionNames().isEmpty(), "Transparent ancestor retained accessible actions");
  actions->doAction(QAccessibleActionInterface::showMenuAction());
  require(calls.size() == 4, "Transparent ancestor dispatched an accessibility action");
  parent->setOpacity(1);
  child->setActiveFocusOnTab(true);
  actions->doAction(QAccessibleActionInterface::setFocusAction());
  require(child->hasActiveFocus() && tree->focusChild() == interface && interface->state().focused,
      "Public Qt setFocus action missed real focus");
  style.accessibilityStateDisabled = true;
  child->setProperty("accessibilityStateDisabled", true); apply(*child, style);
  require(interface->state().disabled && actions->actionNames().isEmpty(), "Disabled registered actions remained available");
  actions->doAction(QAccessibleActionInterface::showMenuAction());
  require(calls.size() == 4, "Disabled action dispatched");
  receiver->invalidate(); delete receiver;
  child->setProperty("accessibilityStateDisabled", false);
  style.accessibilityStateDisabled = false;
  apply(*child, style);
  actions->doAction(QAccessibleActionInterface::showMenuAction());
  require(calls.size() == 4, "Removed receiver retained a callback");
  style.accessible = false; apply(*child, style);
  require(tree->childCount() == 1 && interface->text(QAccessible::Name).isEmpty(), "Hidden View remained in native tree");
  child.reset();
  require(QAccessible::accessibleInterface(identity) == nullptr, "Deleted View retained cached accessible identity");
}
}
int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try {
    contract();
    std::cout << "QT_PLATFORM_ACCESSIBLE_VIEW_OK tree roles actions focus disabled generation delete device=0\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "QT_PLATFORM_ACCESSIBLE_VIEW_FAIL " << error.what() << '\n';
    return 1;
  }
}
