// Deterministic injected Qt input through the real Fabric/Hermes/QPA target.
// This does not prove USB/touch hardware delivery or RNGH recognition.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <react/renderer/core/RawEvent.h>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTouchEvent>
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
struct Observed {
  std::string type;
  double id;
  double target;
  double x;
  double timestamp;
  std::size_t active;
};
void run() {
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(320, 240);
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray(
      "import QtQuick\nItem { visible: true; property color backgroundColor: 'black'; "
      "Rectangle { anchors.fill: parent; color: parent.backgroundColor } }"));
  rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 11);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  rnqnx::fabric::QtTouchDispatcher dispatcher(layer, *window.contentItem());
  adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  adapter.setTouchDispatcher(&dispatcher);
  rnqnx::runtime::QtReactHost host(adapter);
  host.delegate().setTouchDispatcher(&dispatcher);
  host.installComponentProvider();
  host.startEmptySurface(11, 320, 240);
  const char* overridePath = std::getenv("RNQNX_INPUT_FIXTURE");
  const std::string path = overridePath ? overridePath : RNQNX_INPUT_FIXTURE;
  std::ifstream file(path, std::ios::binary);
  require(file.good(), "missing setup fixture");
  const std::string source((std::istreambuf_iterator<char>(file)), {});
  require(host.evaluate(source, path), "fixture evaluation failed");
  require(layer.itemForTag(501) && layer.itemForTag(502), "Fabric fixture not mounted");
  window.show();
  host.pumpEvents();
  QObject input;
  input.installEventFilter(&dispatcher);
  std::vector<Observed> events;
  bool forgetOnEnd = false;
  auto listener = std::make_shared<const facebook::react::EventListener>(
      [&](const facebook::react::RawEvent& event) {
        if (event.type.rfind("topTouch", 0) != 0) return false;
        auto& runtime = host.runtime();
        auto payload = event.eventPayload->asJSIValue(runtime).asObject(runtime);
        auto changed = payload.getPropertyAsObject(runtime, "changedTouches").asArray(runtime);
        auto touch = changed.getValueAtIndex(runtime, 0).asObject(runtime);
        events.push_back({event.type,
            touch.getProperty(runtime, "identifier").asNumber(),
            touch.getProperty(runtime, "target").asNumber(),
            touch.getProperty(runtime, "pageX").asNumber(),
            touch.getProperty(runtime, "timestamp").asNumber(),
            payload.getPropertyAsObject(runtime, "touches").asArray(runtime).size(runtime)});
        if (forgetOnEnd && event.type == "topTouchEnd") dispatcher.forgetTag(501);
        return true;  // Capture production payload; no queued JS test assertions.
      });
  host.scheduler().addEventListener(listener);
  // Remove listener even if an assertion throws, before its captured state dies.
  struct ListenerGuard {
    facebook::react::Scheduler& scheduler;
    std::shared_ptr<const facebook::react::EventListener> listener;
    ~ListenerGuard() { scheduler.removeEventListener(listener); }
  } guard{host.scheduler(), listener};
  const QPointingDevice device(QStringLiteral("input-probe"), 17,
      QInputDevice::DeviceType::TouchScreen, QPointingDevice::PointerType::Finger,
      QInputDevice::Capability::Position, 10, 0);
  const auto touch = [&](QEvent::Type type, const QList<QEventPoint>& points, ulong timestamp) {
    QTouchEvent event(type, &device, Qt::NoModifier, points);
    event.setTimestamp(timestamp);
    QCoreApplication::sendEvent(&input, &event);
  };
  touch(QEvent::TouchBegin, {
      QEventPoint(7, QEventPoint::Pressed, {20, 10}, {20, 10}),
      QEventPoint(8, QEventPoint::Pressed, {20, 90}, {20, 90})}, 100);
  require(events.size() == 2 && events[0].target == 501 && events[1].target == 502,
      "decorated View hit testing failed");
  touch(QEvent::TouchEnd, {
      QEventPoint(7, QEventPoint::Released, {60, 35}, {60, 35}),
      QEventPoint(8, QEventPoint::Stationary, {20, 90}, {20, 90})}, 250);
  require(events.size() == 3 && events.back().type == "topTouchEnd" &&
      events.back().x == 60 && events.back().timestamp == 250 && events.back().active == 1,
      "terminal payload or surviving contact failed");
  QEvent deactivate(QEvent::WindowDeactivate);
  QCoreApplication::sendEvent(&input, &deactivate);
  QCoreApplication::sendEvent(&input, &deactivate);
  require(events.size() == 4 && events.back().type == "topTouchCancel" &&
      events.back().id == 8 && events.back().active == 0, "deactivation cancellation failed");
  touch(QEvent::TouchBegin, {QEventPoint(7, QEventPoint::Pressed, {20, 10}, {20, 10})}, 300);
  touch(QEvent::TouchCancel, {}, 350);
  touch(QEvent::TouchCancel, {}, 350);
  require(events.size() == 6 && events.back().type == "topTouchCancel", "ID reuse/cancel failed");
  touch(QEvent::TouchBegin, {QEventPoint(7, QEventPoint::Pressed, {20, 90}, {20, 90})}, 400);
  dispatcher.setJsResponder(502, true);
  dispatcher.forgetTag(502);
  require(events.size() == 8 && events.back().type == "topTouchCancel", "target removal failed");
  touch(QEvent::TouchBegin, {QEventPoint(9, QEventPoint::Pressed, {300, 200}, {300, 200})}, 450);
  require(events.size() == 8, "removed responder captured outside touch");
  const auto mouse = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent event(type, QPointF(20, 10), QPointF(20, 10), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&input, &event);
  };
  mouse(QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
  require(events.size() == 8, "hover created a touch");
  mouse(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
  QEvent ungrab(QEvent::UngrabMouse);
  QCoreApplication::sendEvent(&input, &ungrab);
  mouse(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
  require(events.size() == 10 && events.back().type == "topTouchCancel", "mouse ungrab failed");
  mouse(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
  forgetOnEnd = true;
  mouse(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
  require(events.size() == 12 && events.back().type == "topTouchEnd", "reentrant mouse release failed");
  host.stopSurface(11);
  std::cout << "RNQNX_INPUT_LIFECYCLE_OK decorated-hit terminal multi-contact cancel deactivate unmount mouse-reentrant\n";
}
}  // namespace
int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try { run(); return 0; }
  catch (const std::exception& error) {
    std::cerr << "RNQNX_INPUT_LIFECYCLE_FAIL " << error.what() << '\n';
    return 1;
  }
}
