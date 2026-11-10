#pragma once

#include "react-native-qnx/runtime/RuntimeTrace.h"

#if RNQNX_ENABLE_FRAME_TRACE
#include <QMetaObject>

class QQuickWindow;

namespace rnqnx::runtime {

/// Internal scoped observation of one window's Qt frameSwapped signal.
/// Construct/destroy on the window owner thread. The callback may run on Qt's
/// render thread; it records no window pointer, JS runtime or surface claim.
/// Disconnect stops future callbacks. An already executing callback retains its
/// own session reference and may finish; freeze() safely rejects later records.
class QtFrameSwapTrace final {
 public:
  QtFrameSwapTrace(QQuickWindow& window, std::shared_ptr<RuntimeTrace> trace);
  ~QtFrameSwapTrace();
  QtFrameSwapTrace(const QtFrameSwapTrace&) = delete;
  QtFrameSwapTrace& operator=(const QtFrameSwapTrace&) = delete;

 private:
  QMetaObject::Connection connection_;
};
} // namespace rnqnx::runtime
#endif
