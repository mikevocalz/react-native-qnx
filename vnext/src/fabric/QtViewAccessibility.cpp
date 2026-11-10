#include "react-native-qnx/fabric/QtViewAccessibility.h"
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include <QAccessible>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QPointer>
#include <QThread>
#include <optional>
#include <cstring>

namespace rnqnx::fabric {
namespace {
std::optional<QAccessible::Role> mappedRole(const QString& role) {
  if (role.isEmpty() || role == QStringLiteral("none")) return QAccessible::Client;
  if (role == QStringLiteral("button") || role == QStringLiteral("togglebutton")) return QAccessible::Button;
  if (role == QStringLiteral("text")) return QAccessible::StaticText;
  if (role == QStringLiteral("header")) return QAccessible::Heading;
  if (role == QStringLiteral("image")) return QAccessible::Graphic;
  if (role == QStringLiteral("checkbox")) return QAccessible::CheckBox;
  if (role == QStringLiteral("radio")) return QAccessible::RadioButton;
  if (role == QStringLiteral("switch")) return QAccessible::Switch;
  if (role == QStringLiteral("combobox")) return QAccessible::ComboBox;
  if (role == QStringLiteral("menu")) return QAccessible::PopupMenu;
  if (role == QStringLiteral("menubar")) return QAccessible::MenuBar;
  if (role == QStringLiteral("menuitem")) return QAccessible::MenuItem;
  if (role == QStringLiteral("dialog") || role == QStringLiteral("alertdialog")) return QAccessible::Dialog;
  if (role == QStringLiteral("list") || role == QStringLiteral("listbox")) return QAccessible::List;
  if (role == QStringLiteral("listitem") || role == QStringLiteral("option")) return QAccessible::ListItem;
  if (role == QStringLiteral("slider") || role == QStringLiteral("adjustable")) return QAccessible::Slider;
  if (role == QStringLiteral("progressbar")) return QAccessible::ProgressBar;
  if (role == QStringLiteral("scrollbar")) return QAccessible::ScrollBar;
  if (role == QStringLiteral("spinbutton")) return QAccessible::SpinBox;
  if (role == QStringLiteral("link")) return QAccessible::Link;
  if (role == QStringLiteral("alert")) return QAccessible::AlertMessage;
  if (role == QStringLiteral("imagebutton") || role == QStringLiteral("keyboardkey")) return QAccessible::Button;
  if (role == QStringLiteral("heading")) return QAccessible::Heading;
  if (role == QStringLiteral("img")) return QAccessible::Graphic;
  if (role == QStringLiteral("tab")) return QAccessible::PageTab;
  if (role == QStringLiteral("tablist")) return QAccessible::PageTabList;
  if (role == QStringLiteral("tabpanel")) return QAccessible::PropertyPage;
  if (role == QStringLiteral("toolbar")) return QAccessible::ToolBar;
  if (role == QStringLiteral("tooltip")) return QAccessible::ToolTip;
  if (role == QStringLiteral("separator")) return QAccessible::Separator;
  if (role == QStringLiteral("group") || role == QStringLiteral("radiogroup") || role == QStringLiteral("viewgroup")) return QAccessible::Grouping;
  if (role == QStringLiteral("tree")) return QAccessible::Tree;
  if (role == QStringLiteral("treeitem")) return QAccessible::TreeItem;
  if (role == QStringLiteral("table") || role == QStringLiteral("grid") || role == QStringLiteral("treegrid")) return QAccessible::Table;
  if (role == QStringLiteral("row")) return QAccessible::Row;
  if (role == QStringLiteral("cell") || role == QStringLiteral("gridcell")) return QAccessible::Cell;
  if (role == QStringLiteral("rowheader")) return QAccessible::RowHeader;
  if (role == QStringLiteral("columnheader")) return QAccessible::ColumnHeader;
  if (role == QStringLiteral("status")) return QAccessible::StatusBar;
  if (role == QStringLiteral("timer")) return QAccessible::Clock;
  if (role == QStringLiteral("summary")) return QAccessible::StaticText;
  if (role == QStringLiteral("search") || role == QStringLiteral("searchbox")) return QAccessible::EditableText;
  if (role == QStringLiteral("scrollview") || role == QStringLiteral("horizontalscrollview")) return QAccessible::Pane;
  return std::nullopt;
}
}
bool applyViewAccessibility(QQuickItem& item, const ViewStyle& style, QString& error) {
  error.clear();
  if (QThread::currentThread() != item.thread()) {
    error = QStringLiteral("Accessibility View requires its Qt owner");
    return false;
  }
  const QPointer<QQuickItem> target(&item);
  const QPointer<QQmlContext> context(qmlContext(&item));
  if (!context) {
    error = QStringLiteral("Accessibility View requires a QML context");
    return false;
  }
  const auto role = mappedRole(style.accessibilityRole);
#if RNQNX_ENABLE_PLATFORM_SERVICES
  const bool previousHidden = item.property("rnqnxAccessibilityHideDescendants").toBool();
  const bool previousBusy = item.property("rnqnxAccessibilityBusy").toBool();
  const QVariant previousExpanded = item.property("rnqnxAccessibilityExpanded");
  const bool previousModal = item.property("rnqnxAccessibilityModal").toBool();
  const QString previousValue = item.property("rnqnxAccessibilityValueText").toString();
  item.setProperty("rnqnxAccessibilityHideDescendants", style.accessibilityHideDescendants);
  if (!target) {
    error = QStringLiteral("Accessibility View was destroyed during exposure update");
    return false;
  }
#endif
  const bool supported = !style.accessible || role.has_value();
  const bool expose = style.accessible && supported;
  const auto write = [&](const char* path, const QVariant& value) {
#if RNQNX_ENABLE_PLATFORM_SERVICES
    if (std::strcmp(path, "Accessible.ignored") == 0)
      return target && context && setPlatformViewAccessibilityIgnored(*target, value.toBool());
#endif
    return target && context && QQmlProperty::write(target, QString::fromLatin1(path), value, context);
  };
  // Hide before updating, including on rejection. Never leave a previous name
  // visible after an unsupported role or a partially failed metadata update.
  if (!write("Accessible.ignored", true)) {
    error = QStringLiteral("Qt Accessible attached properties unavailable");
    return false;
  }
  const bool checkable = expose && style.accessibilityStateCheckable;
  bool ok = true;
  ok &= write("Accessible.name", expose ? style.accessibilityLabel : QString());
  ok &= write("Accessible.description", expose ? style.accessibilityHint : QString());
  ok &= write("Accessible.role", expose ? *role : QAccessible::Client);
  ok &= write("Accessible.selected", expose && style.accessibilityStateSelected);
  ok &= write("Accessible.checkable", checkable);
  ok &= write("Accessible.checked", checkable && style.accessibilityStateChecked);
  ok &= write("Accessible.checkStateMixed", checkable && style.accessibilityStateMixed);
#if RNQNX_ENABLE_PLATFORM_SERVICES
  if (target) target->setProperty("rnqnxAccessibilityBusy", expose && style.accessibilityStateBusy);
  if (target) target->setProperty("rnqnxAccessibilityExpanded", expose ? style.accessibilityStateExpanded : QVariant());
  if (target) target->setProperty("rnqnxAccessibilityModal", expose && style.accessibilityViewIsModal);
  if (target) target->setProperty("rnqnxAccessibilityValueText", expose ? style.accessibilityValue : QString());
#endif
  if (!ok || !target || !context) {
    error = QStringLiteral("Qt Accessible View metadata update failed");
    return false;
  }
  if (!supported) {
    error = QStringLiteral("Unsupported accessibility View role");
    return false;
  }
  if (!write("Accessible.ignored", !expose)) {
    error = QStringLiteral("Qt Accessible View exposure update failed");
    return false;
  }
#if RNQNX_ENABLE_PLATFORM_SERVICES
  const QVariant expanded = expose ? style.accessibilityStateExpanded : QVariant();
  QAccessible::State changed;
  changed.busy = previousBusy != (expose && style.accessibilityStateBusy);
  changed.expandable = previousExpanded.isValid() != expanded.isValid();
  changed.expanded = (previousExpanded.isValid() && previousExpanded.toBool()) != (expanded.isValid() && expanded.toBool());
  changed.collapsed = (previousExpanded.isValid() && !previousExpanded.toBool()) != (expanded.isValid() && !expanded.toBool());
  changed.modal = previousModal != (expose && style.accessibilityViewIsModal);
  if (target && (changed.busy || changed.expandable || changed.expanded || changed.collapsed || changed.modal)) {
    QAccessibleStateChangeEvent event(target, changed);
    QAccessible::updateAccessibility(&event);
  }
  const QString value = expose ? style.accessibilityValue : QString();
  if (target && previousValue != value) {
    QAccessibleValueChangeEvent event(target, value);
    QAccessible::updateAccessibility(&event);
  }
  if (target && previousHidden != style.accessibilityHideDescendants)
    notifyPlatformAccessibilityTreeChanged(*target);
  if (!target || !context) {
    error = QStringLiteral("Accessibility View was destroyed during exposure update");
    return false;
  }
#endif
  return true;
}
}
