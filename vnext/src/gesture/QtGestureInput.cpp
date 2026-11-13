#include "QtGestureInput.h"

#include <QEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QTouchEvent>

namespace rnqnx::gesture {

namespace {
constexpr int kMousePointer = -1;
}

QtGestureInput::QtGestureInput(Registry& registry, QQuickWindow& window)
    : QObject(&window), registry_(registry) {
  deadline_.setSingleShot(true);
  QObject::connect(&deadline_, &QTimer::timeout, this, [this]() {
    registry_.advance(nowMs());
    rearm();
  });
  window.installEventFilter(this);
}

double QtGestureInput::nowMs() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void QtGestureInput::rearm() {
  const auto next = registry_.nextDeadlineMs();
  if (!next) {
    deadline_.stop();
    return;
  }
  deadline_.start(std::max(0, static_cast<int>(*next - nowMs())) + 1);
}

bool QtGestureInput::eventFilter(QObject*, QEvent* event) {
  switch (event->type()) {
    case QEvent::MouseButtonPress: {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() != Qt::LeftButton || mouse->source() != Qt::MouseEventNotSynthesized) break;
      const auto p = mouse->scenePosition();
      registry_.pointerDown(kMousePointer, p.x(), p.y(), nowMs());
      rearm();
      break;
    }
    case QEvent::MouseMove: {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (!(mouse->buttons() & Qt::LeftButton) || mouse->source() != Qt::MouseEventNotSynthesized) break;
      const auto p = mouse->scenePosition();
      registry_.pointerMove(kMousePointer, p.x(), p.y(), nowMs());
      rearm();
      break;
    }
    case QEvent::MouseButtonRelease: {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() != Qt::LeftButton || mouse->source() != Qt::MouseEventNotSynthesized) break;
      const auto p = mouse->scenePosition();
      registry_.pointerUp(kMousePointer, p.x(), p.y(), nowMs());
      rearm();
      break;
    }
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd: {
      auto* touch = static_cast<QTouchEvent*>(event);
      for (const auto& point : touch->points()) {
        const auto p = point.scenePosition();
        switch (point.state()) {
          case QEventPoint::State::Pressed: registry_.pointerDown(point.id(), p.x(), p.y(), nowMs()); break;
          case QEventPoint::State::Updated: registry_.pointerMove(point.id(), p.x(), p.y(), nowMs()); break;
          case QEventPoint::State::Released: registry_.pointerUp(point.id(), p.x(), p.y(), nowMs()); break;
          default: break;
        }
      }
      rearm();
      break;
    }
    case QEvent::TouchCancel:
    case QEvent::WindowDeactivate: {
      // Cancel everything in flight; ids are not enumerable, so the registry
      // drops all of them.
      registry_.pointerCancel(kMousePointer, nowMs());
      for (int id = 0; id < 16; ++id) registry_.pointerCancel(id, nowMs());
      rearm();
      break;
    }
    default:
      break;
  }
  return false;  // observe only
}

}  // namespace rnqnx::gesture
