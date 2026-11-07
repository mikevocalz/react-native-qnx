#include "react-native-qnx/fabric/QtAccessibilityActivation.h"
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtViewAccessibility.h"
#include <QAccessible>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QPointer>
#include <QCoreApplication>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace rnqnx::fabric;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    const bool baseline = app.arguments().contains(QStringLiteral("--without-connection"));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nItem {}", QUrl());
    std::unique_ptr<QObject> object(component.create());
    auto* item = qobject_cast<QQuickItem*>(object.get());
    require(item, "action View unavailable");
    ViewStyle style;
    style.accessible = true; style.accessibilityRole = QStringLiteral("button");
    QString error;
    require(applyViewAccessibility(*item, style, error), "action semantics unavailable");
    auto* iface = QAccessible::queryAccessibleInterface(item);
    require(iface && iface->actionInterface(), "Qt action interface unavailable");
    auto* actions = iface->actionInterface();
    int count = 0;
    auto receiver = std::make_unique<QtAccessibilityActivation>(*item, AccessibilityActivationPolicy{true, true, true, false}, [&] { ++count; });
    if (!baseline) require(receiver->connectPress(error), "public press signal connection failed");
    actions->doAction(QAccessibleActionInterface::pressAction());
    require(count == 1, "Qt press did not reach owned activation receiver");
    require(!receiver->connectPress(error), "duplicate press connection accepted");
    const auto firstGeneration = receiver->generation();
    for (const auto policy : {AccessibilityActivationPolicy{false, true, true, false},
                              AccessibilityActivationPolicy{true, false, true, false},
                              AccessibilityActivationPolicy{true, true, false, false},
                              AccessibilityActivationPolicy{true, true, true, true}}) {
      receiver.reset(new QtAccessibilityActivation(*item, policy, [&] { ++count; }));
      require(receiver->connectPress(error), "guarded receiver unavailable");
      actions->doAction(QAccessibleActionInterface::pressAction());
      require(count == 1, "ineligible action was dispatched");
    }
    receiver.reset(new QtAccessibilityActivation(*item, {true, true, true, false}, [&] { ++count; }));
    require(receiver->generation() != firstGeneration && receiver->connectPress(error), "replacement generation unavailable");
    item->setEnabled(false); actions->doAction(QAccessibleActionInterface::pressAction());
    require(count == 1, "disabled native item dispatched action");
    item->setEnabled(true);
    QMetaObject::invokeMethod(receiver.get(), "activate", Qt::QueuedConnection);
    receiver->invalidate();
    QCoreApplication::processEvents();
    require(count == 1 && !receiver->connectPress(error), "invalidated receiver delivered or rearmed");
    receiver.reset(new QtAccessibilityActivation(*item, {true, true, true, false}, [&] { ++count; }));
    require(receiver->connectPress(error), "restored receiver unavailable");
    QMetaObject::invokeMethod(receiver.get(), "activate", Qt::QueuedConnection);
    QPointer<QtAccessibilityActivation> old(receiver.get());
    receiver.reset();
    QCoreApplication::processEvents();
    require(old.isNull() && count == 1, "deleted receiver delivered queued action");
    // A callback can synchronously delete the receiver; it is never touched afterward.
    auto* selfDeleting = new QtAccessibilityActivation(*item, {true, true, true, false}, [&] {
      ++count;
      delete item->findChild<QtAccessibilityActivation*>();
    });
    require(selfDeleting->connectPress(error), "self-delete receiver unavailable");
    actions->doAction(QAccessibleActionInterface::pressAction());
    require(count == 2, "replacement action failed");
    std::cout << "RNQNX_ACCESSIBILITY_ACTION_NATIVE_OK press guards generation invalidate delete\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_ACCESSIBILITY_ACTION_NATIVE_FAIL " << error.what() << '\n'; return 1;
  }
}
