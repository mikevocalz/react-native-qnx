#include "HostWindowState.h"

#include <QCoreApplication>
#include <QThread>
#include <stdexcept>

namespace rnqnx::runtime {

HostWindowState::HostWindowState() {
  auto* application = QCoreApplication::instance();
  if (!application) {
    throw std::logic_error("HostWindowState requires the QGuiApplication to exist.");
  }
  // This thread owns the object until the move, which is what makes the call legal.
  moveToThread(application->thread());
}

HostWindowState& HostWindowState::instance() {
  static HostWindowState state;
  return state;
}

void HostWindowState::setMinimized(bool minimized) {
  // The flag change and the choice of how to emit it are one step. Without
  // that, two threads can change the flag in one order and queue their
  // emissions in the other, and the last signal listeners see then disagrees
  // with minimized(). The lock covers only the flag and the queueing decision;
  // the signal is emitted after it is released.
  bool direct = false;
  {
    const std::lock_guard<std::mutex> lock(order_);
    if (minimized_.exchange(minimized, std::memory_order_acq_rel) == minimized) return;
    // Direct only on the owner thread with nothing queued ahead of this change,
    // so emissions always leave in the order the flag changed.
    direct = QThread::currentThread() == thread() && queued_.load(std::memory_order_acquire) == 0;
    if (!direct) {
      queued_.fetch_add(1, std::memory_order_acq_rel);
      QMetaObject::invokeMethod(this, [this, minimized] {
        queued_.fetch_sub(1, std::memory_order_acq_rel);
        Q_EMIT minimizedChanged(minimized);
      }, Qt::QueuedConnection);
    }
  }
  if (direct) Q_EMIT minimizedChanged(minimized);
}

void HostWindowState::attach(QGuiApplication& application) {
  if (QThread::currentThread() != thread()) {
    throw std::logic_error("HostWindowState::attach requires the application owner thread.");
  }
  if (attached_) return;
  attached_ = true;
  qtState_.store(static_cast<int>(application.applicationState()), std::memory_order_release);
  connect(&application, &QGuiApplication::applicationStateChanged, this,
          [this](Qt::ApplicationState state) {
            qtState_.store(static_cast<int>(state), std::memory_order_release);
          });
}

}  // namespace rnqnx::runtime
