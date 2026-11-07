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

namespace rnqnx::fabric {
namespace {
std::optional<QAccessible::Role> mappedRole(const QString& role) {
  if (role.isEmpty() || role == QStringLiteral("none")) return QAccessible::Client;
  if (role == QStringLiteral("button")) return QAccessible::Button;
  if (role == QStringLiteral("text")) return QAccessible::StaticText;
  if (role == QStringLiteral("header")) return QAccessible::Heading;
  if (role == QStringLiteral("image")) return QAccessible::Graphic;
  if (role == QStringLiteral("checkbox")) return QAccessible::CheckBox;
  if (role == QStringLiteral("radio")) return QAccessible::RadioButton;
  if (role == QStringLiteral("switch")) return QAccessible::Switch;
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
  const bool supported = !style.accessible || role.has_value();
  const bool expose = style.accessible && supported;
  const auto write = [&](const char* path, const QVariant& value) {
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
  if (!ok) {
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
  return true;
}
}
