// Synthetic Qt accessibility actions through real RN events; no AT consumer.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/runtime/QtRuntimeTaskQueue.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/fabric/QtAccessibilityActivation.h"
#include <react/renderer/core/RawEvent.h>
#include <QAccessible>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QPointer>
#include <QThread>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
QQuickItem* namedView(QQuickItem* item, const QString& name) {
  if (auto* iface = QAccessible::queryAccessibleInterface(item)) {
    if (iface->text(QAccessible::Name) == name) return item;
  }
  for (auto* child : item->childItems()) if (auto* found = namedView(child, name)) return found;
  return nullptr;
}
void run() {
  QQmlEngine engine;
  QQuickWindow window; window.resize(320, 240);
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray("import QtQuick\nItem { property color backgroundColor: 'transparent' }"));
  rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 11);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  rnqnx::runtime::QtReactHost host(adapter);
  host.installTurboModuleBinding(); host.installComponentProvider(); host.installPlatformConstants(); host.installTimers();
  const auto* overridePath = std::getenv("RNQNX_ACCESSIBILITY_ACTION_FIXTURE");
  const std::string path = overridePath ? overridePath : RNQNX_ACCESSIBILITY_ACTION_FIXTURE;
  std::ifstream file(path, std::ios::binary);
  require(file.good(), "missing accessibility action fixture");
  const std::string source((std::istreambuf_iterator<char>(file)), {});
  require(host.evaluate(source, path), "accessibility action fixture evaluation failed");
  const bool intercept = std::getenv("RNQNX_ACCESSIBILITY_ACTION_INTERCEPT_TEST") != nullptr;
  unsigned nativeEvents = 0;
  auto listener = std::make_shared<const facebook::react::EventListener>([&](const facebook::react::RawEvent& event) {
    if (event.type != "accessibilityAction" && event.type != "topAccessibilityAction") return false;
    ++nativeEvents;
    return intercept;
  });
  host.scheduler().addEventListener(listener);
  struct Guard {
    facebook::react::Scheduler& scheduler;
    std::shared_ptr<const facebook::react::EventListener> listener;
    ~Guard() { scheduler.removeEventListener(listener); }
  } guard{host.scheduler(), listener};
  const auto pump = [&] {
    host.runtime().drainMicrotasks(); host.pumpEvents();
    require(host.lastError().isEmpty(), "accessibility action JavaScript runtime error");
    require(host.delegate().lastFailures().empty(), "accessibility action Fabric mount failure");
  };
  const auto until = [&](auto predicate, const char* failure) {
    QElapsedTimer timer; timer.start();
    do { pump(); if (predicate()) return; QThread::msleep(1); } while (timer.elapsed() < 2000);
    require(predicate(), failure);
  };
  const auto settle = [&] { for (int i = 0; i < 20; ++i) pump(); };
  const auto snapshot = [&] { return host.runtime().global().getPropertyAsObject(host.runtime(), "__rnqnxAccessibilitySnapshot"); };
  const auto number = [&](const char* name) { return snapshot().getProperty(host.runtime(), name).asNumber(); };
  const auto call = [&](const char* name, auto&&... args) {
    host.taskQueue()->runSync([&] { auto& runtime = host.runtime(); runtime.global().getPropertyAsFunction(runtime, name).call(runtime, args...); });
  };
  window.show(); host.startSurface(11, "RNQnxAccessibilityActionProbe", 320, 240);
  until([&] { return namedView(window.contentItem(), QStringLiteral("Activation count 0 mode registered")) != nullptr; },
        "React accessibility View did not mount");
  QPointer<QQuickItem> target = namedView(window.contentItem(), QStringLiteral("Activation count 0 mode registered"));
  const auto iface = [&] { return QAccessible::queryAccessibleInterface(target); };
  const auto press = [&] {
    require(target && iface() && iface()->actionInterface(), "actual View action interface missing");
    iface()->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
  };
  require(iface()->role() == QAccessible::Button && number("events") == 0, "initial semantic identity or event mismatch");
  press();
  until([&] { return number("events") == 1; }, "JavaScript accessibility action count mismatch");
  require(nativeEvents == 1 && number("count") == 1 &&
          snapshot().getProperty(host.runtime(), "lastAction").asString(host.runtime()).utf8(host.runtime()) == "activate",
          "accessibility action payload mismatch");
  until([&] { return iface()->text(QAccessible::Name) == QStringLiteral("Activation count 1 mode registered"); }, "React semantic label did not update");
  if (std::getenv("RNQNX_ACCESSIBILITY_ACTION_UNSUPPORTED_TEST")) {
    call("__rnqnxAccessibilityConfigure", "unsupported");
    QElapsedTimer timer; timer.start();
    while (host.delegate().lastFailures().empty() && timer.elapsed() < 2000) {
      host.runtime().drainMicrotasks(); host.pumpEvents(); QThread::msleep(1);
    }
    const auto& failures = host.delegate().lastFailures();
    require(failures.size() == 1 && failures.front().error == rnqnx::fabric::MountError::UnsupportedAccessibility &&
      failures.front().detail == QStringLiteral("Only one registered accessibility activate action is supported"),
      "unsupported registration did not fail explicitly");
    press();
    for (int i = 0; i < 20; ++i) { host.runtime().drainMicrotasks(); host.pumpEvents(); }
    require(number("events") == 1 && nativeEvents == 1, "rejected registration left stale activation");
    host.stopSurface(11);
    std::cout << "RNQNX_ACCESSIBILITY_ACTION_REJECT_OK unsupported-registration\n";
    return;
  }
  for (const auto* mode : {"unregistered", "no-handler", "semantic-disabled", "inaccessible"}) {
    call("__rnqnxAccessibilityConfigure", mode);
    const QString expected = QStringLiteral("Activation count 1 mode ") + QString::fromLatin1(mode);
    until([&] {
      return QString::fromLatin1(mode) == QStringLiteral("inaccessible")
        ? !target->property("accessible").toBool() : iface()->text(QAccessible::Name) == expected;
    }, "React action guard props did not update");
    require(target->isEnabled(), "semantic metadata disabled pointer input");
    press(); settle();
    require(number("events") == 1 && nativeEvents == 1, "ineligible action reached React");
  }
  call("__rnqnxAccessibilityConfigure", "registered");
  until([&] { return iface()->text(QAccessible::Name) == QStringLiteral("Activation count 1 mode registered"); }, "registered action restore failed");
  target->setEnabled(false); press(); settle();
  require(number("events") == 1, "native disabled View delivered action"); target->setEnabled(true);
  auto* receiver = target->findChild<rnqnx::fabric::QtAccessibilityActivation*>();
  require(receiver, "owned activation generation missing");
  const auto generation = receiver->generation();
  QPointer<rnqnx::fabric::QtAccessibilityActivation> previousReceiver(receiver);
  QMetaObject::invokeMethod(receiver, "activate", Qt::QueuedConnection);
  QPointer<QQuickItem> previous(target);
  // Native stop is synchronous. The posted receiver call cannot reach JS after destruction.
  host.stopSurface(11);
  require(previous.isNull() && previousReceiver.isNull(), "surface stop retained action View/receiver"); settle();
  require(number("events") == 1 && nativeEvents == 1, "deleted action generation delivered late event");
  host.startSurface(11, "RNQnxAccessibilityActionProbe", 320, 240);
  until([&] { return namedView(window.contentItem(), QStringLiteral("Activation count 0 mode registered")) != nullptr; }, "React accessibility remount failed");
  target = namedView(window.contentItem(), QStringLiteral("Activation count 0 mode registered"));
  receiver = target->findChild<rnqnx::fabric::QtAccessibilityActivation*>();
  require(receiver && receiver->generation() != generation, "remount reused action generation");
  press();
  until([&] { return number("events") == 2; }, "remounted activation did not reach React");
  require(number("count") == 1 && nativeEvents == 2, "remounted action state leaked");
  host.stopSurface(11); settle();
  std::cout << "RNQNX_ACCESSIBILITY_ACTION_OK react activate guards label teardown remount\n";
}
}
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try { run(); return 0; }
  catch (const std::exception& error) { std::cerr << "RNQNX_ACCESSIBILITY_ACTION_FAIL " << error.what() << '\n'; return 1; }
}
