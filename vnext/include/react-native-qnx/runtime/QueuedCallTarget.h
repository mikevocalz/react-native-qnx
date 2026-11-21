#pragma once

#include <QObject>

#include <functional>

namespace rnqnx::runtime {

/// A receiver for work posted from another thread, run by the receiver's event
/// loop in post order.
///
/// This replaces `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` for the
/// host's thread hand-offs. The behaviour is the same, with one difference that
/// matters for testing: the work is carried in a `QEvent` this code allocates,
/// so the release before `postCall` and the acquire in `event()` cover every
/// byte of it. `invokeMethod` copies its callable inside Qt's queue, where
/// ThreadSanitizer cannot see an ordering edge, so every such call is reported
/// as a race on the callable. The annotations compile to nothing outside a
/// ThreadSanitizer build.
///
/// Destroying the target drops work it has not run yet.
class QueuedCallTarget : public QObject {
 public:
  using QObject::QObject;

  /// Queues `work` on the target's thread and returns. Safe from any thread.
  void postCall(std::function<void()> work);

 protected:
  bool event(QEvent* event) override;
};

}  // namespace rnqnx::runtime
