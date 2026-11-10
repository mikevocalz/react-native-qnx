#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#if RNQNX_ENABLE_DESKTOP_INPUT
#include "react-native-qnx/fabric/QtDesktopInput.h"
#endif

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
    QQuickItem& rootItem,
    bool enableInputLifecycle)
    : layer_(layer),
      rootItem_(rootItem),
      inputLifecycleEnabled_(enableInputLifecycle) {
#if RNQNX_ENABLE_DESKTOP_INPUT
  desktopInput_ = std::make_unique<QtDesktopInput>(layer_, rootItem_);
#endif
}

QtTouchDispatcher::~QtTouchDispatcher() = default;
#if RNQNX_ENABLE_DESKTOP_INPUT
void QtTouchDispatcher::setView(const facebook::react::ShadowView& view) {
  desktopInput_->setView(view);
}
#endif

void QtTouchDispatcher::setEventEmitter(
    Tag tag,
    std::shared_ptr<const facebook::react::TouchEventEmitter> emitter) {
  if (emitter == nullptr) {
    if (inputLifecycleEnabled_) {
      forgetTag(tag);
    } else {
      emitters_.remove(tag);
    }
    return;
  }
  emitters_.insert(tag, std::move(emitter));
}

void QtTouchDispatcher::cancelMouseTouch() {
  if (!mouseTouch_) {
    return;
  }
  const Touch touch = *mouseTouch_;
  mouseTouch_.reset();
  if (responderTag_ == activeTag_) {
    responderTag_ = -1;
  }
  activeTag_ = -1;
  const auto emitter = emitters_.value(touch.target);
  if (emitter != nullptr) {
    TouchEvent event;
    event.changedTouches.insert(touch);
    emitter->onTouchCancel(event);
    ++dispatchedCount_;
  }
}

void QtTouchDispatcher::forgetTag(Tag tag) {
#if RNQNX_ENABLE_DESKTOP_INPUT
  desktopInput_->forgetTag(tag);
#endif
  // Clear capture before dispatch, while the old emitter still exists.
  if (inputLifecycleEnabled_ && responderTag_ == tag) {
    responderTag_ = -1;
  }
  if (activeTag_ == tag) {
    if (inputLifecycleEnabled_) {
      cancelMouseTouch();
    }
    activeTag_ = -1;
  }
  QList<Touch> cancelled;
  for (auto it = activeTouches_.begin(); it != activeTouches_.end();) {
    if (it.value().target == tag) {
      cancelled.append(it.value());
      it = activeTouches_.erase(it);
    } else {
      ++it;
    }
  }
  if (inputLifecycleEnabled_) {
    for (const Touch& touch : cancelled) {
      emitTouchEvent(touch, TouchKind::Cancel);
    }
  }
  emitters_.remove(tag);
}

void QtTouchDispatcher::setJsResponder(Tag tag, bool isResponder) {
  responderTag_ = isResponder ? tag : -1;
}

int QtTouchDispatcher::dispatchedCount() const {
  return dispatchedCount_;
}

Tag QtTouchDispatcher::hitTest(const QPointF& windowPoint) const {
#if RNQNX_ENABLE_DESKTOP_INPUT
  return desktopInput_->hitTest(windowPoint);
#endif
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
  for (const QQuickItem* candidate = target; candidate != &rootItem_;
       candidate = candidate->parentItem()) {
    if (candidate == nullptr) {
      break;
    }
    for (auto iterator = emitters_.constBegin(); iterator != emitters_.constEnd();
         ++iterator) {
      if (layer_.itemForTag(iterator.key()) == candidate) {
        return iterator.key();
      }
    }
    if (!inputLifecycleEnabled_) {
      break;
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
    case TouchKind::Cancel:
      emitter->onTouchCancel(touchEvent);
      break;
  }
  ++dispatchedCount_;
}

bool QtTouchDispatcher::eventFilter(QObject* watched, QEvent* event) {
#if RNQNX_ENABLE_DESKTOP_INPUT
  if (desktopInput_->handleEvent(watched, event)) return true;
#endif
  const auto type = event->type();

  if (inputLifecycleEnabled_ && type == QEvent::TouchCancel) {
    const auto cancelled = std::exchange(activeTouches_, {});
    responderTag_ = -1;
    for (const Touch& touch : cancelled) {
      emitTouchEvent(touch, TouchKind::Cancel);
    }
    return QObject::eventFilter(watched, event);
  }
  if (inputLifecycleEnabled_ &&
      (type == QEvent::WindowDeactivate || type == QEvent::UngrabMouse)) {
    cancelMouseTouch();
    if (activeTouches_.isEmpty()) {
      responderTag_ = -1;
    }
    if (type == QEvent::WindowDeactivate) {
      const auto cancelled = std::exchange(activeTouches_, {});
      responderTag_ = -1;
      for (const Touch& touch : cancelled) {
        emitTouchEvent(touch, TouchKind::Cancel);
      }
    }
    return QObject::eventFilter(watched, event);
  }

  // Classic single-pointer mouse path. Keeping it means desktop tests and
  // pointer-only devices continue to work without synthesising touch events.
  if (type == QEvent::MouseButtonPress || type == QEvent::MouseMove ||
      type == QEvent::MouseButtonRelease) {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    const QPointF windowPoint = mouseEvent->position();
    const double timestampSeconds =
        static_cast<double>(mouseEvent->timestamp()) / 1000.0;

    if (type == QEvent::MouseButtonPress) {
      if (inputLifecycleEnabled_ &&
          (mouseEvent->button() != Qt::LeftButton || mouseTouch_)) {
        return QObject::eventFilter(watched, event);
      }
      activeTag_ = hitTest(windowPoint);
    } else if (inputLifecycleEnabled_ &&
               (!mouseTouch_ ||
                (type == QEvent::MouseButtonRelease &&
                 mouseEvent->button() != Qt::LeftButton))) {
      return QObject::eventFilter(watched, event);
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
      if (inputLifecycleEnabled_) {
        cancelMouseTouch();
      }
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
    if (inputLifecycleEnabled_) {
      touch.timeStamp = facebook::react::HighResTimeStamp::fromDOMHighResTimeStamp(
          static_cast<double>(mouseEvent->timestamp()));
    }
    touch.force = 1.0F;
    mouseTouch_ = touch;

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
        activeTag_ = -1;
        mouseTouch_.reset();
        emitter->onTouchEnd(touchEvent);
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
        if (inputLifecycleEnabled_) {
          touch.timeStamp = facebook::react::HighResTimeStamp::fromDOMHighResTimeStamp(
              static_cast<double>(touchEvent->timestamp()));
        }
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
        if (inputLifecycleEnabled_) {
          touch.timeStamp = facebook::react::HighResTimeStamp::fromDOMHighResTimeStamp(
              static_cast<double>(touchEvent->timestamp()));
        }
        emitTouchEvent(touch, TouchKind::Move);
        break;
      }
      case QEventPoint::Released: {
        auto it = activeTouches_.find(id);
        if (it == activeTouches_.end()) {
          continue;
        }
        Touch touch = it.value();
        if (inputLifecycleEnabled_) {
          touch.pagePoint = {
              .x = static_cast<facebook::react::Float>(scenePos.x()),
              .y = static_cast<facebook::react::Float>(scenePos.y())};
          touch.screenPoint = touch.pagePoint;
          if (QQuickItem* item = layer_.itemForTag(touch.target)) {
            const QPointF local = item->mapFromScene(scenePos);
            touch.offsetPoint = {
                .x = static_cast<facebook::react::Float>(local.x()),
                .y = static_cast<facebook::react::Float>(local.y())};
          }
          touch.timestamp =
              static_cast<facebook::react::Float>(timestampSeconds);
          touch.timeStamp = facebook::react::HighResTimeStamp::fromDOMHighResTimeStamp(
              static_cast<double>(touchEvent->timestamp()));
        }
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
