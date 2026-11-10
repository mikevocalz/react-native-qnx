#pragma once

#include "react-native-qnx/runtime/RuntimeTrace.h"

#if RNQNX_ENABLE_FRAME_TRACE
#include <QMetaObject>

class QQuickWindow;

namespace rnqnx::runtime {

/// Observe on the window owner thread; recording may run on its render thread.
/// The caller owns the trace. Callbacks hold no runtime, Qt item or observer.
/// Scope 0 denotes a window-wide event, not an identified Fabric generation.
class QtFrameTrace final {
 public:
  QtFrameTrace(QQuickWindow& window, const std::shared_ptr<RuntimeTrace>& trace,
               std::int64_t surface = 0, std::uint64_t generation = 0);
  ~QtFrameTrace();
  QtFrameTrace(const QtFrameTrace&) = delete;
  QtFrameTrace& operator=(const QtFrameTrace&) = delete;

 private:
  QMetaObject::Connection connection_;
};

} // namespace rnqnx::runtime
#endif
