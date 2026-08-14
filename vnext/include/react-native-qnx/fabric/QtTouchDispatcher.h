#pragma once

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"

#include <react/renderer/components/view/TouchEventEmitter.h>

#include <QHash>
#include <QObject>

#include <memory>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

namespace rnqnx::fabric {

/// Turns Qt pointer events into React Native touch events.
///
/// React Native has no generic implementation of this: Android reads Choreographer
/// -driven motion events and Apple reads UIKit touches. Qt delivers pointer
/// events to the window, so this filters them there, hit-tests down to the
/// deepest mounted item, and calls the shadow view's `TouchEventEmitter`
/// (`components/view/TouchEventEmitter.h:28-31`).
///
/// Scope today is a single pointer — press, move, release — which is what
/// `Pressable` and the touch handlers need. Multi-touch, pointer capture, and
/// the JS responder negotiation are not implemented, and
/// `docs/component-support.md` records that rather than implying otherwise.
class QtTouchDispatcher final : public QObject {
 public:
  QtTouchDispatcher(QtQuickMountingLayer& layer, QQuickItem& rootItem);

  /// Associates a tag with the shadow view's emitter. The adapter calls this as
  /// mutations arrive; a null emitter unregisters.
  void setEventEmitter(
      Tag tag,
      std::shared_ptr<const facebook::react::TouchEventEmitter> emitter);
  void forgetTag(Tag tag);

  /// Number of touch events delivered, so a test can tell "no handler ran" from
  /// "no event was dispatched".
  int dispatchedCount() const;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  /// Deepest mounted item containing \a windowPoint, or -1.
  Tag hitTest(const QPointF& windowPoint) const;

  QtQuickMountingLayer& layer_;
  QQuickItem& rootItem_;
  QHash<Tag, std::shared_ptr<const facebook::react::TouchEventEmitter>>
      emitters_;
  Tag activeTag_{-1};
  int dispatchedCount_{0};
};

}  // namespace rnqnx::fabric
