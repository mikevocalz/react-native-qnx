#include "react-native-qnx/runtime/QtFrameSwapTrace.h"

#if RNQNX_ENABLE_FRAME_TRACE
#include <QQuickWindow>
#include <utility>

namespace rnqnx::runtime {
QtFrameSwapTrace::QtFrameSwapTrace(QQuickWindow& window, std::shared_ptr<RuntimeTrace> trace) {
  if (!trace) return;
  // Qt documents this signal on the scene graph rendering thread. Record at
  // that boundary, not after a queued hop to the GUI thread. The existing
  // bounded recorder owns synchronization; this callback never touches Qt/JS.
  connection_ = QObject::connect(&window, &QQuickWindow::frameSwapped, &window,
      [trace = std::move(trace)]() noexcept {
        trace->record(TraceStage::FrameSwapped, TracePhase::Instant, trace->nextId());
      }, Qt::DirectConnection);
}

QtFrameSwapTrace::~QtFrameSwapTrace() {
  QObject::disconnect(connection_);
}
} // namespace rnqnx::runtime
#endif
