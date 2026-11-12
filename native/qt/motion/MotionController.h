#pragma once

#include "MotionTypes.h"

#include <QObject>
#include <memory>

class QQuickItem;

namespace rnqnx::motion {

/// Native Qt motion for one item. Construct, call and destroy on the item's
/// GUI owner thread. One controller may own a target at a time. It can run
/// one opacity track and one independent transform track concurrently.
///
/// Detach before releasing a controller when terminal notifications matter.
/// Destruction stops work and removes its transform without calling consumers.
/// Moving either QObject to another thread permanently detaches this controller.
class MotionController final : public QObject {
  Q_OBJECT

 public:
  explicit MotionController(QQuickItem& target, QObject* parent = nullptr);
  ~MotionController() override;

  /// Start from the current value. Replacing the same track completes its
  /// previous operation as Interrupted. Invalid input leaves it untouched.
  quint64 animateOpacity(double opacity, Timing timing = {});
  quint64 animateTransform(const Transform& transform, Timing timing = {});

  /// Cancellation retains the current value; cancelling an idle track is safe.
  void cancel(Track track);
  void pause(Track track);
  void resume(Track track);

  /// Scrub an active or paused track. End time completes it normally.
  void seekTo(Track track, int elapsedMs);

  /// Stop both tracks, remove the added transform and relinquish the target.
  /// Repeated detach calls are safe. The opacity retains its current value.
  void detach();

  State state(Track track) const;
  double opacity() const;
  Transform transform() const;

 Q_SIGNALS:
  /// Exactly one terminal signal per accepted operation while the controller
  /// lives. Native callbacks execute on the owner thread; no JS is called.
  void completed(quint64 operation, rnqnx::motion::Track track,
                 rnqnx::motion::Completion reason);

 protected:
  bool event(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  void requireOwner() const;
  void requireAttached() const;
  quint64 startRun(Track track, Timing timing, double opacity, const Transform& transform);
  void finish(Track track, Completion reason);
  void detachWithReason(Completion reason, bool notify);
};

} // namespace rnqnx::motion
