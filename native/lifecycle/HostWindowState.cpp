#include "HostWindowState.h"

namespace rnqnx::runtime {

HostWindowState& HostWindowState::instance() {
  static HostWindowState state;
  return state;
}

void HostWindowState::setMinimized(bool minimized) {
  if (minimized_.exchange(minimized, std::memory_order_acq_rel) == minimized) return;
  Q_EMIT minimizedChanged(minimized);
}

void HostWindowState::attach(QGuiApplication& application) {
  if (attached_) return;
  attached_ = true;
  qtState_.store(static_cast<int>(application.applicationState()), std::memory_order_release);
  connect(&application, &QGuiApplication::applicationStateChanged, this,
          [this](Qt::ApplicationState state) {
            qtState_.store(static_cast<int>(state), std::memory_order_release);
          });
}

}  // namespace rnqnx::runtime
