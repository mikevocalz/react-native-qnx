#include "react-native-qnx/runtime/QtFrameTrace.h"

#if RNQNX_ENABLE_FRAME_TRACE
#include <QQuickWindow>
#include <QThread>

#include <stdexcept>

namespace rnqnx::runtime {

QtFrameTrace::QtFrameTrace(QQuickWindow& window,
    const std::shared_ptr<RuntimeTrace>& trace,
    std::int64_t surface, std::uint64_t generation) {
  if (!trace) return;
  if (QThread::currentThread() != window.thread()) {
    throw std::invalid_argument("Qt frame tracing must attach on the window owner thread");
  }
  const std::weak_ptr<RuntimeTrace> weak = trace;
  connection_ = QObject::connect(&window, &QQuickWindow::frameSwapped, &window,
      [weak, surface, generation] {
        if (const auto session = weak.lock()) {
          session->record(TraceStage::FrameQueued, TracePhase::Instant,
              session->nextId(), surface, generation);
        }
      }, Qt::DirectConnection);
  if (!connection_) throw std::runtime_error("Cannot observe Qt frame submission");
}

QtFrameTrace::~QtFrameTrace() {
  QObject::disconnect(connection_);
}

} // namespace rnqnx::runtime
#endif
