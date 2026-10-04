#include "react-native-qnx/fabric/QtTouchDispatcher.h"

#include <react/renderer/components/view/TouchEvent.h>

#include <QEvent>
#include <QList>
#include <QMouseEvent>
#include <QQuickItem>
#include <QTouchEvent>

#include <utility>

namespace rnqnx::fabric {
namespace {

using facebook::react::Touch;
using facebook::react::TouchEvent;

/// Builds the React Native touch payload from the current active set.
/// `changed` is the touch this event is about. `targetTouches` is every active
/// touch that started on the same view as `changed`.
TouchEvent makeTouchEvent(
    const QHash<int, Touch>& activeTouches,
    const Touch& changed) {
  TouchEvent event;
  for (auto it = activeTouches.cbegin(); it != activeTouches.cend(); ++it) {
    event.touches.insert(it.value());
    if (it.value().target == changed.target) {
      event.targetTouches.insert(it.value());
    }
  }
  event.changedTouches.insert(changed);
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
  for (auto it = activeTouches_.begin(); it != activeTouches_.end();) {
    if (it.value().target == tag) {
      it = activeTouches_.erase(it);
    } else {
      ++it;
    }
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

void QtTouchDispatcher::emitTouchEvent(const Touch& changed, TouchKind kind) {
  const auto emitter = emitters_.value(changed.target);
  if (emitter == nullptr) {
    return;
  }

  const TouchEvent touchEvent = makeTouchEvent(activeTouches_, changed);
  switch (kind) {
    case TouchKind::Start:
      emitter->onTouchStart(touchEvent);
      break;
    case TouchKind::Move:
      emitter->onTouchMove(touchEvent);
      break;
    case TouchKind::End:
      emitter->onTouchEnd(touchEvent);
      break;
  }
  ++dispatchedCount_;
}

bool QtTouchDispatcher::eventFilter(QObject* watched, QEvent* event) {
  const auto type = event->type();

  // Classic single-pointer mouse path. Keeping it means desktop tests and
  // pointer-only devices continue to work without synthesising touch events.
  if (type == QEvent::MouseButtonPress || type == QEvent::MouseMove ||
      type == QEvent::MouseButtonRelease) {
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

    Touch touch;
    touch.identifier = 0;
    touch.target = activeTag_;
    touch.pagePoint = {
        .x = static_cast<facebook::react::Float>(windowPoint.x()),
        .y = static_cast<facebook::react::Float>(windowPoint.y())};
    touch.screenPoint = touch.pagePoint;
    touch.offsetPoint = {
        .x = static_cast<facebook::react::Float>(
            item->mapFromScene(windowPoint).x()),
        .y = static_cast<facebook::react::Float>(
            item->mapFromScene(windowPoint).y())};
    touch.timestamp = static_cast<facebook::react::Float>(timestampSeconds);
    touch.force = 1.0F;

    // `touches`/`targetTouches` contain only active contacts. On release the
    // ending contact belongs solely in changedTouches, exactly as in the
    // multi-touch path below. Otherwise React never releases its responder.
    TouchEvent touchEvent;
    touchEvent.changedTouches = {touch};
    if (type != QEvent::MouseButtonRelease) {
      touchEvent.touches = {touch};
      touchEvent.targetTouches = {touch};
    }

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

  if (type != QEvent::TouchBegin && type != QEvent::TouchUpdate &&
      type != QEvent::TouchEnd) {
    return QObject::eventFilter(watched, event);
  }

  auto* touchEvent = static_cast<QTouchEvent*>(event);
  const double timestampSeconds =
      static_cast<double>(touchEvent->timestamp()) / 1000.0;

  for (const QEventPoint& point : touchEvent->points()) {
    const QPointF scenePos = point.scenePosition();
    const int id = point.id();

    switch (point.state()) {
      case QEventPoint::Pressed: {
        Tag target = hitTest(scenePos);
        if (responderTag_ >= 0) {
          target = responderTag_;
        }
        if (target < 0) {
          continue;
        }
        QQuickItem* item = layer_.itemForTag(target);
        if (item == nullptr) {
          continue;
        }
        Touch touch;
        touch.identifier = id;
        touch.target = target;
        touch.pagePoint = {
            .x = static_cast<facebook::react::Float>(scenePos.x()),
            .y = static_cast<facebook::react::Float>(scenePos.y())};
        touch.screenPoint = touch.pagePoint;
        touch.offsetPoint = {
            .x = static_cast<facebook::react::Float>(
                item->mapFromScene(scenePos).x()),
            .y = static_cast<facebook::react::Float>(
                item->mapFromScene(scenePos).y())};
        touch.timestamp =
            static_cast<facebook::react::Float>(timestampSeconds);
        touch.force = 1.0F;
        activeTouches_.insert(id, touch);
        emitTouchEvent(touch, TouchKind::Start);
        break;
      }
      case QEventPoint::Updated: {
        auto it = activeTouches_.find(id);
        if (it == activeTouches_.end()) {
          continue;
        }
        QQuickItem* item = layer_.itemForTag(it.value().target);
        if (item == nullptr) {
          continue;
        }
        Touch& touch = it.value();
        touch.pagePoint = {
            .x = static_cast<facebook::react::Float>(scenePos.x()),
            .y = static_cast<facebook::react::Float>(scenePos.y())};
        touch.screenPoint = touch.pagePoint;
        touch.offsetPoint = {
            .x = static_cast<facebook::react::Float>(
                item->mapFromScene(scenePos).x()),
            .y = static_cast<facebook::react::Float>(
                item->mapFromScene(scenePos).y())};
        touch.timestamp =
            static_cast<facebook::react::Float>(timestampSeconds);
        emitTouchEvent(touch, TouchKind::Move);
        break;
      }
      case QEventPoint::Released: {
        auto it = activeTouches_.find(id);
        if (it == activeTouches_.end()) {
          continue;
        }
        const Touch touch = it.value();
        activeTouches_.erase(it);
        emitTouchEvent(touch, TouchKind::End);
        break;
      }
      default:
        break;
    }
  }

  return QObject::eventFilter(watched, event);
}

}  // namespace rnqnx::fabric
