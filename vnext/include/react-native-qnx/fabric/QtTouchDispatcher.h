#pragma once

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"

#include <react/renderer/components/view/TouchEventEmitter.h>

#include <QHash>
#include <QList>
#include <QObject>

#include <react/renderer/components/view/TouchEvent.h>

#include <memory>
#include <optional>

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
/// Mouse and multi-touch contacts are captured until release. The opt-in input
/// lifecycle mode adds cancellation and terminal-coordinate fixes. The JS
/// responder can select the target; gesture recognition is not provided.
class QtTouchDispatcher final : public QObject {
 public:
  QtTouchDispatcher(
      QtQuickMountingLayer& layer,
      QQuickItem& rootItem,
      bool enableInputLifecycle =
#if defined(RNQNX_ENABLE_INPUT_LIFECYCLE) && RNQNX_ENABLE_INPUT_LIFECYCLE
          true
#else
          false
#endif
  );

  /// Associates a tag with the shadow view's emitter. The adapter calls this as
  /// mutations arrive; a null emitter unregisters.
  void setEventEmitter(
      Tag tag,
      std::shared_ptr<const facebook::react::TouchEventEmitter> emitter);
  void forgetTag(Tag tag);

  /// React Native tells the platform which view has claimed the JavaScript
  /// responder (`SchedulerDelegate::schedulerDidSetIsJSResponder`). While one is
  /// set, subsequent pointer events go to it rather than to whatever is under
  /// the pointer, which is what makes a drag that leaves the view still end on
  /// it.
  void setJsResponder(Tag tag, bool isResponder);

  /// Adds the root item of another React surface (the development LogBox).
  /// While it is visible, a press inside it is hit-tested there first and
  /// never reaches the views underneath.
  void addRootItem(QQuickItem& rootItem);

  /// A native item drawn over the React surface (the development menu and
  /// error screen). While it is visible, presses inside it are left to Qt
  /// Quick and not dispatched to React.
  void addInputShield(QQuickItem& shield);

  /// Number of touch events delivered, so a test can tell "no handler ran" from
  /// "no event was dispatched".
  int dispatchedCount() const;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  /// Deepest mounted item containing \a windowPoint, or -1.
  Tag hitTest(const QPointF& windowPoint) const;

  enum class TouchKind { Start, Move, End, Cancel };
  void cancelMouseTouch();
  void emitTouchEvent(
      const facebook::react::Touch& changed,
      TouchKind kind);

  QtQuickMountingLayer& layer_;
  QQuickItem& rootItem_;
  QList<QQuickItem*> extraRoots_;
  QList<QQuickItem*> shields_;
  QHash<Tag, std::shared_ptr<const facebook::react::TouchEventEmitter>>
      emitters_;
  QHash<int, facebook::react::Touch> activeTouches_;
  const bool inputLifecycleEnabled_;
  Tag activeTag_{-1};
  std::optional<facebook::react::Touch> mouseTouch_;
  Tag responderTag_{-1};
  int dispatchedCount_{0};
};

}  // namespace rnqnx::fabric
