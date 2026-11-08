#include "QtApplicationLifecycle.h"

#include <QPointer>
#include <stdexcept>

namespace rnqnx::runtime {

QtApplicationLifecycle::QtApplicationLifecycle(QGuiApplication& application, QObject* parent)
    : QObject(parent), owner_(application.thread()) {
  requireOwner();
  qRegisterMetaType<LifecycleSnapshot>();
  snapshot_.visibility = visibilityFor(application.applicationState());
  stateConnection_ = connect(&application, &QGuiApplication::applicationStateChanged,
      this, &QtApplicationLifecycle::record);
  quitConnection_ = connect(&application, &QCoreApplication::aboutToQuit,
      this, &QtApplicationLifecycle::close);
  connect(&application, &QObject::destroyed, this, &QtApplicationLifecycle::close);
}

void QtApplicationLifecycle::requireOwner() const {
  if (QThread::currentThread() != owner_ || thread() != owner_) {
    throw std::logic_error("Lifecycle observation requires the application owner thread.");
  }
}

ApplicationVisibility QtApplicationLifecycle::visibilityFor(Qt::ApplicationState state) noexcept {
  switch (state) {
    case Qt::ApplicationActive: return ApplicationVisibility::Active;
    case Qt::ApplicationInactive: return ApplicationVisibility::Inactive;
    case Qt::ApplicationHidden: return ApplicationVisibility::Hidden;
    case Qt::ApplicationSuspended: return ApplicationVisibility::Suspended;
    default: return ApplicationVisibility::Unknown;
  }
}

LifecycleSnapshot QtApplicationLifecycle::snapshot() const {
  requireOwner();
  return snapshot_;
}

void QtApplicationLifecycle::close() {
  requireOwner();
  if (snapshot_.closed) return;
  snapshot_.closed = true;
  pending_.clear();
  disconnect(stateConnection_);
  disconnect(quitConnection_);
  emit observationClosed();
}

void QtApplicationLifecycle::record(Qt::ApplicationState state) {
  requireOwner();
  if (snapshot_.closed) return;
  pending_.push_back(visibilityFor(state));
  if (draining_) return;
  draining_ = true;
  while (!snapshot_.closed && !pending_.empty()) {
    const auto next = pending_.front();
    pending_.pop_front();
    if (next == snapshot_.visibility) continue;
    snapshot_.visibility = next;
    ++snapshot_.sequence;
    // Direct consumers can close or delete the observer during delivery.
    // Reentrant transitions are drained after the current signal so observers
    // receive increasing sequence numbers, rather than nested stale snapshots.
    const QPointer<QtApplicationLifecycle> alive(this);
    emit stateChanged(snapshot_);
    if (!alive) return;
  }
  draining_ = false;
}

}  // namespace rnqnx::runtime
