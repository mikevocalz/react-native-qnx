#pragma once

#include "react-native-qnx/runtime/QtReactHost.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class QQuickWindow;

namespace rnqnx::worklets {

/// Counters the probes and the gallery read. All atomics; any thread.
struct QtWorkletsStats {
  std::atomic<std::uint64_t> uiJobsRun{0};
  std::atomic<std::uint64_t> framesRequested{0};
  std::atomic<std::uint64_t> framesDelivered{0};
  std::atomic<std::uint64_t> jsPosts{0};
};

/// Hosts react-native-worklets on Qt.
///
/// Threads: the React Native JS runtime stays on the GUI thread (this host's
/// owner). The UI Worklet Runtime runs on its own thread, created here. The two
/// meet through `QtRuntimeTaskQueue` (UI to JS) and a queued post (JS to UI).
/// Qt Quick items are only changed on the GUI thread.
///
/// The upstream sources are not part of this library: `RNQNX_WORKLETS_SOURCE_DIR`
/// points at an installed react-native-worklets package (see
/// cmake/RnqnxWorklets.cmake).
class QtWorkletsPlatform {
 public:
  /// Registers the `WorkletsModule` TurboModule and a teardown hook on `host`,
  /// before `host.installTurboModuleBinding()`. `window` supplies frames.
  /// The platform lives until the host is destroyed.
  static std::shared_ptr<QtWorkletsPlatform> install(
      runtime::QtReactHost& host, QQuickWindow* window);

  virtual ~QtWorkletsPlatform() = default;

  virtual const QtWorkletsStats& stats() const noexcept = 0;
  virtual bool uiThreadIsCurrent() const noexcept = 0;
  /// "gui", "ui-worklet" or "other"; for diagnostics.
  virtual std::string currentThreadRole() const = 0;
  using FrameCallback = std::function<void(double /*timestampMs*/)>;

  /// UI thread only. Runs `callback` once at the next Qt Quick frame
  /// (`afterAnimating`), with a monotonic millisecond timestamp.
  virtual void requestFrame(FrameCallback callback) = 0;
  /// Any thread. Runs `job` on the UI-worklet thread; false after shutdown.
  virtual bool postToUi(std::function<void()> job) = 0;
  /// Any thread. Runs `job` on the GUI thread, the only thread that may change
  /// a Qt Quick item. Dropped after shutdown.
  virtual void postToGui(std::function<void()> job) = 0;
  /// A CallInvoker that is safe from any thread: it posts to the host's JS owner
  /// queue. React Native's own invoker runs JS inline on the caller here.
  virtual std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker() const = 0;
  /// Monotonic milliseconds on the same clock as frame timestamps.
  virtual double animationTimestampMs() const = 0;

  /// Stops the UI thread and frame bridge. Idempotent. The host calls it at
  /// teardown; tests may call it earlier to exercise cancel.
  virtual void shutdown() = 0;
};

}  // namespace rnqnx::worklets
