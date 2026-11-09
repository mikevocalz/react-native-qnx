// Linux/macOS Qt host evidence only: this does not qualify QNX physical HID.
#include "react-native-qnx/views/QtWheelDirectionFilter.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTouchEvent>
#include <QWheelEvent>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using rnqnx::views::QtWheelDirectionFilter;

void check(bool value, const char* expression, int line) {
  if (!value) {
    throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + expression);
  }
}
#define CHECK(expression) check((expression), #expression, __LINE__)

class RecordingWindow final : public QQuickWindow {
 public:
  int wheelCount{0};
  int mouseCount{0};
  int touchCount{0};
  bool acceptWheel{true};
  std::unique_ptr<QWheelEvent> wheel;
  std::unique_ptr<QMouseEvent> mouse;
  std::unique_ptr<QTouchEvent> touch;

 protected:
  bool event(QEvent* event) override {
    if (event->type() == QEvent::Wheel) {
      ++wheelCount;
      wheel.reset(static_cast<QWheelEvent*>(event)->clone());
      event->setAccepted(acceptWheel);
      return acceptWheel;
    }
    if (event->type() == QEvent::MouseMove) {
      ++mouseCount;
      mouse.reset(static_cast<QMouseEvent*>(event)->clone());
      return true;
    }
    if (event->type() == QEvent::TouchUpdate) {
      ++touchCount;
      touch.reset(static_cast<QTouchEvent*>(event)->clone());
      return true;
    }
    return QQuickWindow::event(event);
  }
};

void defaultLeavesWheelUnchanged() {
  RecordingWindow window;
  QtWheelDirectionFilter filter(window);
  QWheelEvent event({27.5, 41.25}, {327.5, 141.25}, {3, -9}, {120, -240},
      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QCoreApplication::sendEvent(&window, &event);
  CHECK(window.wheelCount == 1);
  CHECK(window.wheel->angleDelta() == event.angleDelta());
  CHECK(window.wheel->pixelDelta() == event.pixelDelta());
}

void metadataAndAcceptanceSurvive(bool initialAccepted, bool receiverAccepted) {
  RecordingWindow window;
  window.acceptWheel = receiverAccepted;
  QtWheelDirectionFilter filter(window, true);
  const QPointingDevice device(QStringLiteral("wheel test"), 17,
      QInputDevice::DeviceType::TouchPad, QPointingDevice::PointerType::Finger,
      QInputDevice::Capability::Position, 1, 3);
  QWheelEvent event({27.5, 41.25}, {327.5, 141.25}, {3, -9}, {120, -240},
      Qt::LeftButton | Qt::MiddleButton, Qt::ShiftModifier | Qt::ControlModifier,
      Qt::ScrollMomentum, true, Qt::MouseEventSynthesizedBySystem, &device);
  event.setTimestamp(982731);
  event.setAccepted(initialAccepted);
  QCoreApplication::sendEvent(&window, &event);
  CHECK(window.wheelCount == 1);  // Also catches redispatch without the recursion guard.
  const QWheelEvent& received = *window.wheel;
  CHECK(received.angleDelta() == QPoint(-120, 240));
  CHECK(received.pixelDelta() == QPoint(-3, 9));
  CHECK(received.position() == event.position());
  CHECK(received.scenePosition() == event.scenePosition());
  CHECK(received.globalPosition() == event.globalPosition());
  CHECK(received.buttons() == event.buttons());
  CHECK(received.modifiers() == event.modifiers());
  CHECK(received.phase() == event.phase());
  CHECK(received.source() == event.source());
  CHECK(received.inverted() == event.inverted());
  CHECK(received.pointingDevice() == &device);
  CHECK(received.timestamp() == event.timestamp());
  CHECK(received.spontaneous() == event.spontaneous());
  CHECK(received.pointCount() == event.pointCount());
  CHECK(received.points().first().timestamp() == event.points().first().timestamp());
  CHECK(received.isAccepted() == initialAccepted);
  CHECK(event.isAccepted() == receiverAccepted);
  CHECK(event.angleDelta() == QPoint(120, -240));
  CHECK(event.pixelDelta() == QPoint(3, -9));
}

void filterOnlyHandlesItsWindowAndLifetime() {
  RecordingWindow window;
  {
    QtWheelDirectionFilter filter(window, true);
    RecordingWindow other;
    other.installEventFilter(&filter);
    QWheelEvent event({20, 20}, {20, 20}, {}, {0, 120},
        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&other, &event);
    CHECK(other.wheelCount == 1);
    CHECK(other.wheel->angleDelta() == QPoint(0, 120));
  }
  QWheelEvent event({20, 20}, {20, 20}, {}, {0, 120},
      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QCoreApplication::sendEvent(&window, &event);
  CHECK(window.wheelCount == 1);
  CHECK(window.wheel->angleDelta() == QPoint(0, 120));

  auto ownedWindow = std::make_unique<QQuickWindow>();
  auto filter = std::make_unique<QtWheelDirectionFilter>(*ownedWindow, true);
  ownedWindow.reset();
  filter.reset();  // QPointer must make window-first teardown safe.
}

void nonWheelMotionIsUnchanged() {
  RecordingWindow window;
  QtWheelDirectionFilter filter(window, true);
  QMouseEvent mouse(QEvent::MouseMove, {52, 21}, {152, 121},
      Qt::NoButton, Qt::LeftButton, Qt::ShiftModifier);
  mouse.setTimestamp(7781);
  QCoreApplication::sendEvent(&window, &mouse);
  CHECK(window.mouseCount == 1);
  CHECK(window.mouse->position() == mouse.position());
  CHECK(window.mouse->globalPosition() == mouse.globalPosition());
  CHECK(window.mouse->timestamp() == mouse.timestamp());
  CHECK(window.mouse->buttons() == mouse.buttons());

  const QPointingDevice device(QStringLiteral("touch test"), 18,
      QInputDevice::DeviceType::TouchScreen, QPointingDevice::PointerType::Finger,
      QInputDevice::Capability::Position, 1, 0);
  const QEventPoint point(2, QEventPoint::State::Updated, {39, 61}, {139, 161});
  QTouchEvent touch(QEvent::TouchUpdate, &device, Qt::NoModifier, {point});
  touch.setTimestamp(7782);
  QCoreApplication::sendEvent(&window, &touch);
  CHECK(window.touchCount == 1);
  CHECK(window.touch->points().first().scenePosition() == point.scenePosition());
  CHECK(window.touch->points().first().globalPosition() == point.globalPosition());
  CHECK(window.touch->timestamp() == touch.timestamp());
  CHECK(window.wheelCount == 0);
}

void zeroAndExtremeDeltasAreSafe() {
  RecordingWindow window;
  QtWheelDirectionFilter filter(window, true);
  const int minimum = std::numeric_limits<int>::min();
  const int maximum = std::numeric_limits<int>::max();
  QWheelEvent event({20, 20}, {20, 20}, {minimum, maximum}, {},
      Qt::NoButton, Qt::NoModifier, Qt::ScrollBegin, false);
  QCoreApplication::sendEvent(&window, &event);
  CHECK(window.wheelCount == 1);
  CHECK(window.wheel->angleDelta().isNull());
  CHECK(window.wheel->pixelDelta() == QPoint(maximum, -maximum));
  CHECK(window.wheel->phase() == Qt::ScrollBegin);
}

enum class FilterMode { None, Default, Inverted };

double flickableMovement(FilterMode mode, bool horizontal, int delta) {
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(240, 240);
  QQmlComponent component(&engine);
  component.setData(R"QML(
import QtQuick
Flickable {
    width: 240; height: 240
    contentWidth: 1200; contentHeight: 1200
    contentX: 400; contentY: 400
    flickableDirection: Flickable.HorizontalAndVerticalFlick
    boundsBehavior: Flickable.StopAtBounds
    Rectangle { width: 1200; height: 1200; color: "steelblue" }
}
)QML", QUrl(QStringLiteral("qrc:/wheel-direction-test.qml")));
  CHECK(component.isReady());
  std::unique_ptr<QObject> object(component.create());
  auto* flickable = qobject_cast<QQuickItem*>(object.get());
  CHECK(flickable != nullptr);
  flickable->setParentItem(window.contentItem());
  std::unique_ptr<QtWheelDirectionFilter> filter;
  if (mode != FilterMode::None) {
    filter = std::make_unique<QtWheelDirectionFilter>(window, mode == FilterMode::Inverted);
  }
  window.show();
  QCoreApplication::processEvents();
  const char* property = horizontal ? "contentX" : "contentY";
  const double before = flickable->property(property).toDouble();
  QWheelEvent wheel({120, 120}, window.mapToGlobal(QPointF(120, 120)), {},
      horizontal ? QPoint(delta, 0) : QPoint(0, delta),
      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  wheel.setTimestamp(1000);
  QCoreApplication::sendEvent(&window, &wheel);
  CHECK(wheel.isAccepted());
  QElapsedTimer deadline;
  deadline.start();
  // Wait on the actual Qt animation progressing, with a failure deadline.
  while (flickable->property(property).toDouble() == before && deadline.elapsed() < 1000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  return flickable->property(property).toDouble() - before;
}

void realWindowFlickableDirection() {
  for (const bool horizontal : {false, true}) {
    for (const int delta : {-120, 120}) {
      const double unfiltered = flickableMovement(FilterMode::None, horizontal, delta);
      const double defaultFilter = flickableMovement(FilterMode::Default, horizontal, delta);
      const double inverted = flickableMovement(FilterMode::Inverted, horizontal, delta);
      CHECK(unfiltered * delta < 0);
      CHECK(defaultFilter * delta < 0);
      CHECK(inverted * delta > 0);
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try {
    defaultLeavesWheelUnchanged();
    for (const bool initialAccepted : {false, true}) {
      for (const bool receiverAccepted : {false, true}) {
        metadataAndAcceptanceSurvive(initialAccepted, receiverAccepted);
      }
    }
    filterOnlyHandlesItsWindowAndLifetime();
    nonWheelMotionIsUnchanged();
    zeroAndExtremeDeltasAreSafe();
    realWindowFlickableDirection();
    std::cout << "RNQNX_WHEEL_DIRECTION_OK default metadata acceptance lifetime mouse touch flickable\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_WHEEL_DIRECTION_FAIL " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
