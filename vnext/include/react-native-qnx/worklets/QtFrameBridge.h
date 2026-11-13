#pragma once

#include <QPointer>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class QObject;
class QQuickWindow;

namespace rnqnx::worklets {

class QtUiThread;

/// Delivers one `requestAnimationFrame` batch per Qt Quick frame.
///
/// Phase: `QQuickWindow::afterAnimating`, emitted on the GUI thread after
/// animations advance and before the scene graph syncs. The callbacks then run
/// on the UI-worklet thread, so a worklet's property writes reach the GUI
/// thread as queued calls and land in the following frame's sync, never in the
/// render thread.
///
/// A request marks the window dirty (`QQuickWindow::update`), so an idle app
/// does not tick. There is no timer: a window that is hidden or not exposed
/// produces no frames and therefore no callbacks. Multiple windows are not
/// supported; the bridge follows exactly one.
///
/// Timestamps are `steady_clock` milliseconds, sampled on the GUI thread at the
/// signal. They are monotonic and not wall time.
class QtFrameBridge final {
 public:
  using FrameCallback = std::function<void(double /*timestampMs*/)>;

  /// Must be created on the GUI thread, which owns `window`.
  QtFrameBridge(QQuickWindow* window, QtUiThread& uiThread);
  ~QtFrameBridge();

  QtFrameBridge(const QtFrameBridge&) = delete;
  QtFrameBridge& operator=(const QtFrameBridge&) = delete;

  /// UI thread only. Runs `callback` at the next frame, once.
  void request(FrameCallback callback);

  /// Any thread. Runs `job` on the GUI thread (queued). Dropped after shutdown.
  /// Qt Quick items may only be changed from there.
  void postToGui(std::function<void()> job);

  /// GUI thread. Stops delivery; callbacks not yet run are dropped. Idempotent.
  void shutdown();

  std::uint64_t framesDelivered() const noexcept { return delivered_.load(); }
  std::uint64_t framesSignalled() const noexcept { return signalled_.load(); }

 private:
  void onAfterAnimating();

  QPointer<QQuickWindow> window_;
  QtUiThread& uiThread_;
  std::atomic<QObject*> guiContext_{nullptr};
  std::unique_ptr<QObject> guiContextOwner_;
  // UI thread only.
  std::vector<FrameCallback> pending_;
  std::atomic<bool> wanted_{false};
  std::atomic<bool> shutdown_{false};
  std::atomic<std::uint64_t> delivered_{0};
  std::atomic<std::uint64_t> signalled_{0};
};

}  // namespace rnqnx::worklets
