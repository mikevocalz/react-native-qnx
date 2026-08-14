#include "react-native-qnx/fabric/QtTouchDispatcher.h"

#include <react/renderer/components/view/TouchEvent.h>

#include <QEvent>
#include <QList>
#include <QMouseEvent>
#include <QQuickItem>

#include <utility>

namespace rnqnx::fabric {
namespace {

using facebook::react::Touch;
using facebook::react::TouchEvent;

/// Builds the single-pointer touch event React Native expects. All three lists
/// carry the same touch because there is one pointer: `touches` is everything
/// currently down, `changedTouches` is what this event is about, and
/// `targetTouches` is what started on the target (TouchEvent.h:20-38).
TouchEvent makeTouchEvent(Tag target, const QPointF& windowPoint, const QPointF& localPoint, double timestampSeconds) {
  Touch touch;
  touch.identifier = 0;
  touch.target = target;
  touch.pagePoint = {
      .x = static_cast<facebook::react::Float>(windowPoint.x()),
      .y = static_cast<facebook::react::Float>(windowPoint.y())};
  touch.screenPoint = touch.pagePoint;
  touch.offsetPoint = {
      .x = static_cast<facebook::react::Float>(localPoint.x()),
      .y = static_cast<facebook::react::Float>(localPoint.y())};
  touch.timestamp = static_cast<facebook::react::Float>(timestampSeconds);
  touch.force = 1.0F;

  TouchEvent event;
  event.touches = {touch};
  event.changedTouches = {touch};
  event.targetTouches = {touch};
  return event;
}

}  // namespace

QtTouchDispatcher::QtTouchDispatcher(
    QtQuickMountingLayer& layer,
    QQuickItem& rootItem)
    : layer_(layer), rootItem_(rootItem) {}

void QtTouchDispatcher::setEventEmitter(
    Tag tag,
    std::shared_ptr<const facebook::react::TouchEventEmitter> emitter) {
  if (emitter == nullptr) {
    emitters_.remove(tag);
    return;
  }
  emitters_.insert(tag, std::move(emitter));
}

void QtTouchDispatcher::forgetTag(Tag tag) {
  emitters_.remove(tag);
  if (activeTag_ == tag) {
    activeTag_ = -1;
  }
}

void QtTouchDispatcher::setJsResponder(Tag tag, bool isResponder) {
  responderTag_ = isResponder ? tag : -1;
}

int QtTouchDispatcher::dispatchedCount() const {
  return dispatchedCount_;
}

Tag QtTouchDispatcher::hitTest(const QPointF& windowPoint) const {
  // Walk down from the root taking the last (topmost) child that contains the
  // point, which matches Qt's own stacking order.
  const QQuickItem* target = nullptr;
  const QQuickItem* current = &rootItem_;

  while (true) {
    const QQuickItem* next = nullptr;
    const QList<QQuickItem*> children = current->childItems();
    for (auto iterator = children.crbegin(); iterator != children.crend();
         ++iterator) {
      QQuickItem* child = *iterator;
      if (!child->isVisible()) {
        continue;
      }
      const QPointF local = child->mapFromScene(windowPoint);
      if (local.x() >= 0 && local.y() >= 0 && local.x() < child->width() &&
          local.y() < child->height()) {
        next = child;
        break;
      }
    }
    if (next == nullptr) {
      break;
    }
    target = next;
    current = next;
  }

  if (target == nullptr) {
    return -1;
  }

  // Reverse-map the item to its tag. The registry is small — one entry per
  // mounted view — and this runs once per pointer event.
  for (auto iterator = emitters_.constBegin(); iterator != emitters_.constEnd();
       ++iterator) {
    if (layer_.itemForTag(iterator.key()) == target) {
      return iterator.key();
    }
  }
  return -1;
}

bool QtTouchDispatcher::eventFilter(QObject* watched, QEvent* event) {
  const auto type = event->type();
  if (type != QEvent::MouseButtonPress && type != QEvent::MouseMove &&
      type != QEvent::MouseButtonRelease) {
    return QObject::eventFilter(watched, event);
  }

  auto* mouseEvent = static_cast<QMouseEvent*>(event);
  const QPointF windowPoint = mouseEvent->position();
  const double timestampSeconds =
      static_cast<double>(mouseEvent->timestamp()) / 1000.0;

  if (type == QEvent::MouseButtonPress) {
    activeTag_ = hitTest(windowPoint);
  }

  // A claimed responder wins over the hit test for the rest of the gesture.
  if (responderTag_ >= 0) {
    activeTag_ = responderTag_;
  }

  if (activeTag_ < 0) {
    return QObject::eventFilter(watched, event);
  }

  const auto emitter = emitters_.value(activeTag_);
  QQuickItem* item = layer_.itemForTag(activeTag_);
  if (emitter == nullptr || item == nullptr) {
    activeTag_ = -1;
    return QObject::eventFilter(watched, event);
  }

  const TouchEvent touchEvent = makeTouchEvent(
      activeTag_, windowPoint, item->mapFromScene(windowPoint), timestampSeconds);

  switch (type) {
    case QEvent::MouseButtonPress:
      emitter->onTouchStart(touchEvent);
      break;
    case QEvent::MouseMove:
      emitter->onTouchMove(touchEvent);
      break;
    case QEvent::MouseButtonRelease:
      emitter->onTouchEnd(touchEvent);
      activeTag_ = -1;
      break;
    default:
      break;
  }

  ++dispatchedCount_;
  return QObject::eventFilter(watched, event);
}

}  // namespace rnqnx::fabric
