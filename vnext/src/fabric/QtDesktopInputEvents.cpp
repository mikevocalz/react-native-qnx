#include "react-native-qnx/fabric/QtDesktopInput.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <folly/dynamic.h>

namespace rnqnx::fabric::desktop {
namespace {
std::string keyName(const QKeyEvent& event) {
  switch (event.key()) {
    case Qt::Key_Return: case Qt::Key_Enter: return "Enter";
    case Qt::Key_Space: return " ";
    case Qt::Key_Escape: return "Escape";
    case Qt::Key_Menu: return "ContextMenu";
    case Qt::Key_Tab: case Qt::Key_Backtab: return "Tab";
    case Qt::Key_Left: return "ArrowLeft";
    case Qt::Key_Right: return "ArrowRight";
    case Qt::Key_Up: return "ArrowUp";
    case Qt::Key_Down: return "ArrowDown";
    case Qt::Key_Home: return "Home";
    case Qt::Key_End: return "End";
    case Qt::Key_PageUp: return "PageUp";
    case Qt::Key_PageDown: return "PageDown";
    case Qt::Key_Backspace: return "Backspace";
    case Qt::Key_Delete: return "Delete";
    default: return event.text().isEmpty() ? "Unidentified" : event.text().toStdString();
  }
}
folly::dynamic keyPayload(Tag tag, const QKeyEvent& event) {
  return folly::dynamic::object("target", tag)("key", keyName(event))
      ("type", event.type() == QEvent::KeyPress ? "keydown" : "keyup")
      ("repeat", event.isAutoRepeat())("timestamp", static_cast<double>(event.timestamp()))
      ("ctrlKey", event.modifiers().testFlag(Qt::ControlModifier))
      ("altKey", event.modifiers().testFlag(Qt::AltModifier))
      ("shiftKey", event.modifiers().testFlag(Qt::ShiftModifier))
      ("metaKey", event.modifiers().testFlag(Qt::MetaModifier));
}
}  // namespace

facebook::react::PointerEvent pointerEvent(
    const QMouseEvent* event, const QQuickItem& item, QPointF point) {
  using facebook::react::Float;
  facebook::react::PointerEvent result{};
  const QPointF local = item.mapFromScene(point);
  const QPointF screen = event ? event->globalPosition() : item.mapToGlobal(local);
  result.pointerId = 1;
  result.pointerType = "mouse";
  result.isPrimary = true;
  result.width = result.height = 1;
  result.clientPoint = {static_cast<Float>(point.x()), static_cast<Float>(point.y())};
  result.offsetPoint = {static_cast<Float>(local.x()), static_cast<Float>(local.y())};
  result.screenPoint = {static_cast<Float>(screen.x()), static_cast<Float>(screen.y())};
  result.button = -1;
  if (event) {
    result.buttons = (event->buttons().testFlag(Qt::LeftButton) ? 1 : 0) |
        (event->buttons().testFlag(Qt::RightButton) ? 2 : 0) |
        (event->buttons().testFlag(Qt::MiddleButton) ? 4 : 0);
    result.pressure = result.buttons ? 0.5F : 0.0F;
    result.ctrlKey = event->modifiers().testFlag(Qt::ControlModifier);
    result.altKey = event->modifiers().testFlag(Qt::AltModifier);
    result.shiftKey = event->modifiers().testFlag(Qt::ShiftModifier);
    result.metaKey = event->modifiers().testFlag(Qt::MetaModifier);
    result.timeStamp = facebook::react::HighResTimeStamp::fromDOMHighResTimeStamp(
        static_cast<double>(event->timestamp()));
  }
  return result;
}

void keyEvent(const facebook::react::TouchEventEmitter& emitter,
              Tag tag, const QKeyEvent& event) {
  emitter.dispatchEvent(event.type() == QEvent::KeyPress ? "keyDown" : "keyUp",
      keyPayload(tag, event), facebook::react::RawEvent::Category::Discrete);
}

void keyboardClick(const facebook::react::TouchEventEmitter& emitter,
                   Tag tag, const QKeyEvent& event) {
  // A non-pointer click is Pressability's supported keyboard/accessibility
  // activation path. Pointer clicks are deliberately not emitted a second time.
  emitter.dispatchEvent("click", keyPayload(tag, event),
      facebook::react::RawEvent::Category::Discrete);
}
}  // namespace rnqnx::fabric::desktop
