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
  if (minimized_.exchange(minimized, std::memory_order_acq_rel) == minimized) return;
  // Direct on the owner thread, queued from any other, in call order.
  QMetaObject::invokeMethod(this, [this, minimized] { Q_EMIT minimizedChanged(minimized); },
      Qt::AutoConnection);
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
