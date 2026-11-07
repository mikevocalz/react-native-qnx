// Injected Qt input into a production-bundled React Native Pressable. This is
// separate from the raw Fabric payload probe and does not prove physical HID.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <react/renderer/core/RawEvent.h>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <QTouchEvent>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void run() {
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(320, 240);
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray(
      "import QtQuick\nItem { property color backgroundColor: 'black'; "
      "Rectangle { anchors.fill: parent; color: parent.backgroundColor } }"));
  rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 11);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  rnqnx::fabric::QtTouchDispatcher dispatcher(layer, *window.contentItem());
  adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  adapter.setTouchDispatcher(&dispatcher);
  rnqnx::runtime::QtReactHost host(adapter);
  host.delegate().setTouchDispatcher(&dispatcher);
  host.installTurboModuleBinding();
  host.installComponentProvider();
  host.installPlatformConstants();
  host.installTimers();
  if (std::getenv("RNQNX_PRESSABLE_DISABLE_REMOVED_TARGET_FIX") != nullptr) {
    // Causality control: use this exact binary and bundle while restoring the
    // pinned renderer's original null-target cancellation behavior.
    host.runtime().global().setProperty(
        host.runtime(), "__RNQNX_INPUT_LIFECYCLE__", false);
  }
  const char* overridePath = std::getenv("RNQNX_PRESSABLE_FIXTURE");
  const std::string path = overridePath ? overridePath : RNQNX_PRESSABLE_FIXTURE;
  std::ifstream file(path, std::ios::binary);
  require(file.good(), "missing Pressable fixture");
  const std::string source((std::istreambuf_iterator<char>(file)), {});
  require(host.evaluate(source, path), "Pressable fixture evaluation failed");
  const auto pump = [&]() {
    host.runtime().drainMicrotasks();
    host.pumpEvents();
    require(host.lastError().isEmpty(), "JavaScript runtime error");
    require(host.delegate().lastFailures().empty(), "Fabric mount failure");
  };
  const auto until = [&](auto ready, const char* message) {
    QElapsedTimer elapsed;
    elapsed.start();
    do {
      pump();
      if (ready()) return;
      QThread::msleep(1);
    } while (elapsed.elapsed() < 2000);
    require(ready(), message);
  };
  const auto number = [&](const char* name) {
    auto& runtime = host.runtime();
    return runtime.global().getProperty(runtime, name).asNumber();
  };
  const auto events = [&]() {
    auto& runtime = host.runtime();
    auto values = runtime.global().getPropertyAsObject(runtime, "__rnqnxPressableEvents").asArray(runtime);
    std::vector<std::string> result;
    for (std::size_t i = 0; i < values.size(runtime); ++i) {
      result.push_back(values.getValueAtIndex(runtime, i).asString(runtime).utf8(runtime));
    }
    return result;
  };
  const auto settle = [&]() {
    QElapsedTimer elapsed;
    elapsed.start();
    // Exercise Pressability's delayed press-out timer as well as queued beats.
    while (elapsed.elapsed() < 180) { pump(); QThread::msleep(1); }
  };
  window.show();
  host.startSurface(11, "RNQnxPressableProbe", 320, 240);
  until([&]() { return number("__rnqnxPressableMounts") == 1 && layer.mountedCount() == 4; },
      "React component did not mount");
  QObject input;
  input.installEventFilter(&dispatcher);
  const bool intercept = std::getenv("RNQNX_PRESSABLE_INTERCEPT_TEST") != nullptr;
  auto listener = std::make_shared<const facebook::react::EventListener>(
      [&](const facebook::react::RawEvent& event) {
        return intercept && event.type.rfind("topTouch", 0) == 0;
      });
  host.scheduler().addEventListener(listener);
  struct ListenerGuard {
    facebook::react::Scheduler& scheduler;
    std::shared_ptr<const facebook::react::EventListener> listener;
    ~ListenerGuard() { scheduler.removeEventListener(listener); }
  } guard{host.scheduler(), listener};
  const auto mouse = [&](bool down, QPointF point = {40, 40}) {
    QMouseEvent event(down ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
        point, point, Qt::LeftButton, down ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&input, &event);
  };
  mouse(true);
  until([&]() { return events().size() == 1; }, "Pressable onPressIn missing");
  require(events() == std::vector<std::string>{"in"}, "initial press-in mismatch");
  // The pinned Pressability implementation delays press-out for very short
  // presses. Hold beyond that minimum to assert its synchronous release order.
  settle();
  mouse(false);
  until([&]() { return events().size() == 3; }, "Pressable tap callbacks missing");
  require(events() == std::vector<std::string>{"in", "out", "press"}, "tap callback order mismatch");

  const QPointingDevice device(QStringLiteral("pressable-probe"), 17,
      QInputDevice::DeviceType::TouchScreen, QPointingDevice::PointerType::Finger,
      QInputDevice::Capability::Position, 10, 0);
  QTouchEvent down(QEvent::TouchBegin, &device, Qt::NoModifier,
      {QEventPoint(7, QEventPoint::Pressed, {40, 40}, {40, 40})});
  down.setTimestamp(300);
  QCoreApplication::sendEvent(&input, &down);
  until([&]() { return events().size() == 4; }, "touch press-in missing");
  QTouchEvent cancel(QEvent::TouchCancel, &device, Qt::NoModifier, QList<QEventPoint>{});
  cancel.setTimestamp(350);
  QCoreApplication::sendEvent(&input, &cancel);
  QCoreApplication::sendEvent(&input, &cancel);
  until([&]() { return events().size() == 5; }, "touch cancel press-out missing");
  settle();
  require(events() == std::vector<std::string>{"in", "out", "press", "in", "out"},
      "cancellation activated Pressable or duplicated callbacks");

  mouse(true);
  until([&]() { return events().size() == 6; }, "deactivate press-in missing");
  QEvent deactivate(QEvent::WindowDeactivate);
  QCoreApplication::sendEvent(&input, &deactivate);
  QCoreApplication::sendEvent(&input, &deactivate);
  mouse(false);
  until([&]() { return events().size() == 7; }, "deactivate press-out missing");
  settle();
  require(events() == std::vector<std::string>{"in", "out", "press", "in", "out", "in", "out"},
      "deactivation activated Pressable or duplicated callbacks");
  mouse(true, {40, 120});
  mouse(false, {40, 120});
  settle();
  require(events().size() == 7, "disabled Pressable activated");

  mouse(true);
  until([&]() { return events().size() == 8; }, "unmount press-in missing");
  host.stopSurface(11);
  until([&]() { return number("__rnqnxPressableUnmounts") == 1; }, "React cleanup did not run");
  settle();
  const auto afterUnmount = events();
  require(std::count(afterUnmount.begin(), afterUnmount.end(), "press") == 1,
      "unmount activated Pressable");
  host.startSurface(11, "RNQnxPressableProbe", 320, 240);
  until([&]() { return number("__rnqnxPressableMounts") == 2 && layer.mountedCount() == 4; },
      "React component did not remount");
  mouse(true);
  until([&]() { return events().size() == afterUnmount.size() + 1; }, "remount press-in missing");
  settle();
  mouse(false);
  until([&]() { return events().size() == afterUnmount.size() + 3; }, "remount tap missing");
  const auto final = events();
  require(std::vector<std::string>(final.end() - 3, final.end()) ==
      std::vector<std::string>{"in", "out", "press"}, "remount tap order mismatch");
  host.stopSurface(11);
  until([&]() { return number("__rnqnxPressableUnmounts") == 2; }, "final React cleanup missing");
  std::cout << "RNQNX_PRESSABLE_OK tap cancel deactivate disabled unmount remount callbacks="
            << events().size() << '\n';
}
} // namespace
int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try { run(); return 0; }
  catch (const std::exception& error) {
    std::cerr << "RNQNX_PRESSABLE_FAIL " << error.what() << '\n';
    return 1;
  }
}
