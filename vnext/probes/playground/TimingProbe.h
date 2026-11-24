#pragma once

// Startup and frame-swap recorder for the performance budgets. Built on Linux
// and macOS hosts, and on QNX since 2026-11-23 (QNX build not yet run, see
// docs/evidence/pi400-timing-reader-2026-11-23).
// Contract: docs/nitro/API_CONTRACTS.md, "S4 frame and startup timing probe".
// No JavaScript API. A swap instant is the steady_clock reading taken when
// QQuickWindow::frameSwapped is delivered, not a display presentation time.
// On QNX the resident set is read from procfs page data and the peak is the
// highest of the samples taken every 100 ms (rssSource "sampled-procfs"), not an
// OS-reported peak.

#include <QEvent>
#include <QObject>
#include <QString>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QQuickWindow;
class QTimer;

namespace rnqnx::probe {

struct TimingConfig {
  int settleMs = 500;
  int wheelNotches = 0;
  int wheelGapMs = 200;
  // Pointer drag script (drawer-drag workload). 0 steps means no drag. One
  // script is an open drag from the left edge followed by a drag back.
  int dragSteps = 0;
  int dragStepGapMs = 16;
  // Observe mode: after settling, wait this long and record when pointer events
  // from the platform (a real or Screen-injected device) first and last reach
  // the window. 0 means off. The probe posts nothing.
  int observeMs = 0;
  // Internal. Not on the command line; tests lower it to exercise overflow.
  int frameCapacity = 4096;

  // Parses the three command line values. An empty string keeps the default.
  // On failure returns nullopt and sets `error`.
  static std::optional<TimingConfig> fromStrings(
      const QString& settleMs, const QString& wheelNotches, const QString& wheelGapMs,
      QString* error, const QString& dragSteps = {}, const QString& dragStepGapMs = {},
      const QString& observeMs = {});
};

class TimingProbe final : public QObject {
 public:
  // `origin` is the steady_clock reading main() takes as its first statement.
  // Connects to `window` with Qt::DirectConnection before the window is shown.
  TimingProbe(QQuickWindow& window, TimingConfig config,
              std::chrono::steady_clock::time_point origin);
  ~TimingProbe() override;
  TimingProbe(const TimingProbe&) = delete;
  TimingProbe& operator=(const TimingProbe&) = delete;

  void markBundleEvaluated() noexcept;  // GUI thread, once
  void markSurfaceStarted() noexcept;   // GUI thread, once, after startSurface returns

  // Waits for the first swap after markSurfaceStarted, settles, posts the wheel
  // script, snapshots, then calls `done` once on the GUI thread. `done(false)`
  // means no swap arrived within 30 s of start().
  void start(std::function<void(bool)> done);

  // Valid after `done(true)`. Returns an error string, empty on success. The
  // destination must not exist; a failed write leaves nothing there.
  QString writeJson(const QString& path) const;

  struct State;

 private:
  void onFrameControl();
#if defined(__QNX__)
  void startMemorySampling();
  void sampleMemory();
#endif
  void postNotch();
  void postDragEvent();
  void finishObserve();
  void finish(bool ok);
  bool eventFilter(QObject* watched, QEvent* event) override;

  QQuickWindow& window_;
  TimingConfig config_;
  std::shared_ptr<State> state_;
  QMetaObject::Connection direct_;
  QMetaObject::Connection control_;
  std::function<void(bool)> done_;
#if defined(__QNX__)
  QTimer* memoryTimer_ = nullptr;  // owned by this object
#endif
  enum class Phase { Idle, WaitingFirstFrame, Settling, Wheel, Drag, Observe, Done };
  Phase phase_ = Phase::Idle;
  int notchesSent_ = 0;
  int dragEventsSent_ = 0;
  int inputEvents_ = 0;
  std::int64_t inputFirstNs_ = -1;
  std::int64_t inputLastNs_ = -1;
  std::int64_t wheelFirstNs_ = -1;
  std::int64_t wheelLastNs_ = -1;

  // Snapshot taken once at finish.
  std::vector<std::int64_t> frameNs_;
  std::uint64_t totalFrames_ = 0;
  std::int64_t firstFrameNs_ = -1;
  std::int64_t bundleEvaluatedNs_ = -1;
  std::int64_t surfaceStartedNs_ = -1;
  std::optional<std::uint64_t> rssKiB_;
  std::optional<std::uint64_t> peakRssKiB_;
};

}  // namespace rnqnx::probe
