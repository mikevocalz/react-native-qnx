#include "TimingProbe.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryFile>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#elif defined(__linux__)
#include <QFile>
#elif defined(__QNX__)
#include "QnxProcessMemory.h"
#endif
#include <unistd.h>

namespace rnqnx::probe {

struct TimingProbe::State {
  State(std::size_t capacity, std::chrono::steady_clock::time_point originIn)
      : records(capacity), origin(originIn) {}
  std::vector<std::int64_t> records;
  const std::chrono::steady_clock::time_point origin;
  std::atomic<std::uint64_t> total{0};
  std::atomic<bool> stopped{false};
  std::atomic<bool> surfaceStarted{false};
  std::atomic<std::int64_t> firstFrameNs{-1};
#if defined(__QNX__)
  // GUI thread only. QNX has no OS peak, so the peak is the highest sample.
  PeakTracker residentPeak;
#endif

  std::int64_t nowNs() const noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now() - origin)
        .count();
  }
};

namespace {

constexpr int kWatchdogMs = 30000;
// QNX only: how often the resident set is sampled while the workload runs.
[[maybe_unused]] constexpr int kMemorySampleMs = 100;

std::optional<int> parseRange(const QString& text, int fallback, int low, int high,
                              const char* name, QString* error) {
  if (text.isEmpty()) return fallback;
  bool ok = false;
  const int value = text.toInt(&ok, 10);
  if (!ok || value < low || value > high) {
    *error = QStringLiteral("%1 must be an integer from %2 to %3, got '%4'")
                 .arg(QLatin1String(name))
                 .arg(low)
                 .arg(high)
                 .arg(text);
    return std::nullopt;
  }
  return value;
}

double toMs(std::int64_t ns) { return std::round(static_cast<double>(ns) / 1000.0) / 1000.0; }

// Resident and peak resident set in KiB, or nullopt where no reader exists.
struct Memory {
  std::optional<std::uint64_t> rssKiB;
  std::optional<std::uint64_t> peakKiB;
};

#if defined(__APPLE__)
Memory readMemory() {
  Memory memory;
  mach_task_basic_info info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info),
                &count) == KERN_SUCCESS) {
    memory.rssKiB = static_cast<std::uint64_t>(info.resident_size) / 1024;
  }
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
    memory.peakKiB = static_cast<std::uint64_t>(usage.ru_maxrss) / 1024;  // bytes on macOS
  }
  return memory;
}
#elif defined(__linux__)
Memory readMemory() {
  Memory memory;
  QFile status(QStringLiteral("/proc/self/status"));
  if (!status.open(QIODevice::ReadOnly)) return memory;
  // procfs reports size 0, so atEnd() and seek() are not reliable: read it whole.
  const QList<QByteArray> lines = status.readAll().split('\n');
  const auto field = [&lines](const char* key) -> std::optional<std::uint64_t> {
    for (const QByteArray& line : lines) {
      if (!line.startsWith(key)) continue;
      const QList<QByteArray> parts = line.simplified().split(' ');
      bool ok = false;
      const auto value = parts.value(1).toULongLong(&ok);
      return ok ? std::optional<std::uint64_t>(value) : std::nullopt;  // kB
    }
    return std::nullopt;
  };
  memory.rssKiB = field("VmRSS:");
  memory.peakKiB = field("VmHWM:");
  return memory;
}
#elif defined(__QNX__)
// Resident set only: the peak comes from the samples TimingProbe takes while it runs.
Memory readMemory() {
  Memory memory;
  memory.rssKiB = readOwnResidentKiB();
  return memory;
}
#else
Memory readMemory() { return {}; }
#endif

QString osName() {
#if defined(__APPLE__)
  return QStringLiteral("darwin");
#elif defined(__linux__)
  return QStringLiteral("linux");
#elif defined(__QNX__)
  return QStringLiteral("qnx");
#else
  return QStringLiteral("unsupported");
#endif
}

QString backendName(const QQuickWindow& window) {
  QSGRendererInterface* renderer = const_cast<QQuickWindow&>(window).rendererInterface();
  if (renderer == nullptr) return QStringLiteral("unknown");
  switch (renderer->graphicsApi()) {
    case QSGRendererInterface::Software: return QStringLiteral("software");
    case QSGRendererInterface::OpenGL: return QStringLiteral("opengl");
    case QSGRendererInterface::Vulkan: return QStringLiteral("vulkan");
    case QSGRendererInterface::Metal: return QStringLiteral("metal");
    case QSGRendererInterface::Direct3D11: return QStringLiteral("d3d11");
    case QSGRendererInterface::Direct3D12: return QStringLiteral("d3d12");
    case QSGRendererInterface::Null: return QStringLiteral("null");
    default: return QStringLiteral("other");
  }
}

}  // namespace

std::optional<TimingConfig> TimingConfig::fromStrings(
    const QString& settleMs, const QString& wheelNotches, const QString& wheelGapMs,
    QString* error, const QString& dragSteps, const QString& dragStepGapMs,
    const QString& observeMs) {
  TimingConfig config;
  const auto settle = parseRange(settleMs, config.settleMs, 0, 60000, "--timing-settle-ms", error);
  if (!settle) return std::nullopt;
  const auto notches =
      parseRange(wheelNotches, config.wheelNotches, 0, 1000, "--timing-wheel-notches", error);
  if (!notches) return std::nullopt;
  const auto gap = parseRange(wheelGapMs, config.wheelGapMs, 1, 5000, "--timing-wheel-gap-ms", error);
  if (!gap) return std::nullopt;
  const auto drag = parseRange(dragSteps, config.dragSteps, 0, 500, "--timing-drag-steps", error);
  if (!drag) return std::nullopt;
  const auto dragGap =
      parseRange(dragStepGapMs, config.dragStepGapMs, 1, 1000, "--timing-drag-gap-ms", error);
  if (!dragGap) return std::nullopt;
  const auto observe = parseRange(observeMs, config.observeMs, 0, 120000, "--timing-observe-ms", error);
  if (!observe) return std::nullopt;
  if (*drag > 0 && *observe > 0) {
    *error = QStringLiteral("--timing-drag-steps and --timing-observe-ms cannot be combined");
    return std::nullopt;
  }
  if (*notches > 0 && (*drag > 0 || *observe > 0)) {
    *error = QStringLiteral("the wheel script cannot be combined with a drag or observe run");
    return std::nullopt;
  }
  config.settleMs = *settle;
  config.wheelNotches = *notches;
  config.wheelGapMs = *gap;
  config.dragSteps = *drag;
  config.dragStepGapMs = *dragGap;
  config.observeMs = *observe;
  return config;
}

TimingProbe::TimingProbe(QQuickWindow& window, TimingConfig config,
                         std::chrono::steady_clock::time_point origin)
    : window_(window),
      config_(config),
      state_(std::make_shared<State>(static_cast<std::size_t>(config.frameCapacity), origin)) {
  // The functor owns the state, so a handler running on the render thread
  // never touches freed memory, whatever order the probe and window die in.
  direct_ = QObject::connect(
      &window_, &QQuickWindow::frameSwapped, &window_,
      [state = state_]() {
        if (state->stopped.load(std::memory_order_acquire)) return;
        const std::int64_t ns = state->nowNs();
        const std::uint64_t n = state->total.load(std::memory_order_acquire);
        if (n < state->records.size()) state->records[n] = ns;
        if (state->firstFrameNs.load(std::memory_order_relaxed) < 0 &&
            state->surfaceStarted.load(std::memory_order_acquire)) {
          state->firstFrameNs.store(ns, std::memory_order_release);
        }
        state->total.store(n + 1, std::memory_order_release);
      },
      Qt::DirectConnection);
  // Control flow only. It carries no timestamp.
  control_ = QObject::connect(&window_, &QQuickWindow::frameSwapped, this,
                              [this]() { onFrameControl(); }, Qt::AutoConnection);
}

TimingProbe::~TimingProbe() {
  QObject::disconnect(direct_);
  QObject::disconnect(control_);
  state_->stopped.store(true, std::memory_order_release);
}

void TimingProbe::markBundleEvaluated() noexcept { bundleEvaluatedNs_ = state_->nowNs(); }

void TimingProbe::markSurfaceStarted() noexcept {
  surfaceStartedNs_ = state_->nowNs();
  state_->surfaceStarted.store(true, std::memory_order_release);
}

void TimingProbe::start(std::function<void(bool)> done) {
  done_ = std::move(done);
  phase_ = Phase::WaitingFirstFrame;
#if defined(__QNX__)
  startMemorySampling();
#endif
  QTimer::singleShot(kWatchdogMs, this, [this]() {
    if (phase_ == Phase::WaitingFirstFrame) finish(false);
  });
  // A swap may already have landed between markSurfaceStarted and start.
  onFrameControl();
}

#if defined(__QNX__)
void TimingProbe::startMemorySampling() {
  sampleMemory();
  memoryTimer_ = new QTimer(this);
  memoryTimer_->setInterval(kMemorySampleMs);
  QObject::connect(memoryTimer_, &QTimer::timeout, this, [this]() { sampleMemory(); });
  memoryTimer_->start();
}

void TimingProbe::sampleMemory() {
  if (const auto kib = readOwnResidentKiB()) state_->residentPeak.record(*kib);
}
#endif

void TimingProbe::onFrameControl() {
  if (phase_ != Phase::WaitingFirstFrame) return;
  if (state_->firstFrameNs.load(std::memory_order_acquire) < 0) return;
  phase_ = Phase::Settling;
  QTimer::singleShot(config_.settleMs, this, [this]() {
    if (config_.dragSteps > 0) {
      phase_ = Phase::Drag;
      postDragEvent();
    } else if (config_.observeMs > 0) {
      phase_ = Phase::Observe;
      window_.installEventFilter(this);
      QTimer::singleShot(config_.observeMs, this, [this]() { finishObserve(); });
    } else {
      phase_ = Phase::Wheel;
      postNotch();
    }
  });
}

void TimingProbe::postNotch() {
  if (phase_ != Phase::Wheel) return;
  if (notchesSent_ >= config_.wheelNotches) {
    finish(true);
    return;
  }
  const QPointF position(window_.width() / 2.0, window_.height() / 2.0);
  const QPointF global = window_.mapToGlobal(position);
  // posted, not sent: the event runs on the next loop turn, outside this timer.
  QCoreApplication::postEvent(
      &window_, new QWheelEvent(position, global, QPoint(), QPoint(0, -120), Qt::NoButton,
                                Qt::NoModifier, Qt::NoScrollPhase, false));
  const std::int64_t postedNs = state_->nowNs();
  if (wheelFirstNs_ < 0) wheelFirstNs_ = postedNs;
  wheelLastNs_ = postedNs;
  ++notchesSent_;
  QTimer::singleShot(config_.wheelGapMs, this, [this]() { postNotch(); });
}

bool TimingProbe::eventFilter(QObject* watched, QEvent* event) {
  if (phase_ == Phase::Observe && watched == &window_) {
    switch (event->type()) {
      case QEvent::MouseButtonPress:
      case QEvent::MouseMove:
      case QEvent::MouseButtonRelease:
      case QEvent::TouchBegin:
      case QEvent::TouchUpdate:
      case QEvent::TouchEnd: {
        const std::int64_t ns = state_->nowNs();
        if (inputFirstNs_ < 0) inputFirstNs_ = ns;
        inputLastNs_ = ns;
        ++inputEvents_;
        break;
      }
      default: break;
    }
  }
  return false;
}

void TimingProbe::finishObserve() {
  if (phase_ != Phase::Observe) return;
  window_.removeEventFilter(this);
  // No pointer events means nothing moved: the run has no animated window.
  finish(inputEvents_ > 0);
}

// One drag script: press on the left edge, move right across 60 % of the
// window in dragSteps moves, release; then the same path back. Events are
// posted, so they take the platform-independent mouse route the real device
// route ends in. Index 0 is the press and the last index is the release.
void TimingProbe::postDragEvent() {
  if (phase_ != Phase::Drag) return;
  const int steps = config_.dragSteps;
  const int perLeg = steps + 2;  // press, steps moves, release
  const int total = perLeg * 2;
  if (dragEventsSent_ >= total) {
    finish(true);
    return;
  }
  const int leg = dragEventsSent_ / perLeg;  // 0 open, 1 close
  const int index = dragEventsSent_ % perLeg;
  const double span = window_.width() * 0.6;
  const double y = window_.height() / 2.0;
  const double edge = 6.0;
  const double start = leg == 0 ? edge : edge + span;
  const double end = leg == 0 ? edge + span : edge;
  QEvent::Type type = QEvent::MouseMove;
  Qt::MouseButton button = Qt::NoButton;
  Qt::MouseButtons buttons = Qt::LeftButton;
  double x = start;
  if (index == 0) {
    type = QEvent::MouseButtonPress;
    button = Qt::LeftButton;
  } else if (index == perLeg - 1) {
    type = QEvent::MouseButtonRelease;
    button = Qt::LeftButton;
    buttons = Qt::NoButton;
    x = end;
  } else {
    x = start + (end - start) * index / static_cast<double>(steps);
  }
  const QPointF position(x, y);
  QCoreApplication::postEvent(
      &window_, new QMouseEvent(type, position, window_.mapToGlobal(position), button, buttons,
                                Qt::NoModifier));
  const std::int64_t postedNs = state_->nowNs();
  if (inputFirstNs_ < 0) inputFirstNs_ = postedNs;
  inputLastNs_ = postedNs;
  ++inputEvents_;
  ++dragEventsSent_;
  // Hold 300 ms between the open leg and the close leg so the drawer settles.
  const int gap = (index == perLeg - 1 && leg == 0) ? 300 : config_.dragStepGapMs;
  QTimer::singleShot(gap, this, [this]() { postDragEvent(); });
}

void TimingProbe::finish(bool ok) {
  if (phase_ == Phase::Done) return;
  phase_ = Phase::Done;
  if (ok) {
    totalFrames_ = state_->total.load(std::memory_order_acquire);
    firstFrameNs_ = state_->firstFrameNs.load(std::memory_order_acquire);
    const auto stored = static_cast<std::size_t>(std::min<std::uint64_t>(totalFrames_, state_->records.size()));
    frameNs_.assign(state_->records.begin(), state_->records.begin() + static_cast<std::ptrdiff_t>(stored));
    const Memory memory = readMemory();
    rssKiB_ = memory.rssKiB;
    peakRssKiB_ = memory.peakKiB;
#if defined(__QNX__)
    sampleMemory();
    peakRssKiB_ = state_->residentPeak.peak();
#endif
  }
  state_->stopped.store(true, std::memory_order_release);
#if defined(__QNX__)
  if (memoryTimer_ != nullptr) memoryTimer_->stop();
#endif
  if (done_) {
    auto done = std::move(done_);
    done(ok);
  }
}

QString TimingProbe::writeJson(const QString& path) const {
  QJsonArray frames;
  for (const std::int64_t ns : frameNs_) frames.append(toMs(ns));
  const std::uint64_t dropped = totalFrames_ - frameNs_.size();
  QJsonObject root;
  root.insert(QStringLiteral("schema"), 1);
  root.insert(QStringLiteral("scope"),
              QStringLiteral("steady_clock from the first statement of main() to the first "
                             "QQuickWindow::frameSwapped after the surface started; excludes "
                             "dynamic loading before main(); a swap is not a display presentation"));
  root.insert(QStringLiteral("clock"), QStringLiteral("steady_clock"));
  root.insert(QStringLiteral("platform"), QGuiApplication::platformName());
  root.insert(QStringLiteral("renderBackend"), backendName(window_));
  root.insert(QStringLiteral("os"), osName());
  root.insert(QStringLiteral("bundleEvaluatedMs"), toMs(bundleEvaluatedNs_));
  root.insert(QStringLiteral("surfaceStartedMs"), toMs(surfaceStartedNs_));
  root.insert(QStringLiteral("firstFrameMs"), toMs(firstFrameNs_));
  root.insert(QStringLiteral("frameMs"), frames);
  root.insert(QStringLiteral("totalFrames"), static_cast<double>(totalFrames_));
  root.insert(QStringLiteral("droppedFrames"), static_cast<double>(dropped));
  root.insert(QStringLiteral("wheelNotchesSent"), notchesSent_);
  root.insert(QStringLiteral("wheelFirstMs"),
              wheelFirstNs_ >= 0 ? QJsonValue(toMs(wheelFirstNs_)) : QJsonValue(QJsonValue::Null));
  root.insert(QStringLiteral("wheelLastMs"),
              wheelLastNs_ >= 0 ? QJsonValue(toMs(wheelLastNs_)) : QJsonValue(QJsonValue::Null));
  const QString inputKind = config_.dragSteps > 0 ? QStringLiteral("posted-drag")
                            : config_.observeMs > 0 ? QStringLiteral("observed-pointer")
                            : notchesSent_ > 0      ? QStringLiteral("posted-wheel")
                                                    : QStringLiteral("none");
  root.insert(QStringLiteral("inputKind"), inputKind);
  root.insert(QStringLiteral("inputEvents"), inputEvents_);
  root.insert(QStringLiteral("inputFirstMs"),
              inputFirstNs_ >= 0 ? QJsonValue(toMs(inputFirstNs_)) : QJsonValue(QJsonValue::Null));
  root.insert(QStringLiteral("inputLastMs"),
              inputLastNs_ >= 0 ? QJsonValue(toMs(inputLastNs_)) : QJsonValue(QJsonValue::Null));
  root.insert(QStringLiteral("rssKiB"),
              rssKiB_ ? QJsonValue(static_cast<double>(*rssKiB_)) : QJsonValue(QJsonValue::Null));
  root.insert(QStringLiteral("peakRssKiB"),
              peakRssKiB_ ? QJsonValue(static_cast<double>(*peakRssKiB_)) : QJsonValue(QJsonValue::Null));
#if defined(__QNX__)
  const QString rssSource = rssKiB_ ? QStringLiteral("sampled-procfs") : QStringLiteral("unavailable");
#else
  const QString rssSource = rssKiB_ ? QStringLiteral("os") : QStringLiteral("unavailable");
#endif
  root.insert(QStringLiteral("rssSource"), rssSource);
  const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);

  const QFileInfo target(path);
  QTemporaryFile temporary(target.absoluteDir().filePath(QStringLiteral(".timing-XXXXXX")));
  if (!temporary.open()) return QStringLiteral("cannot create a temporary file beside %1").arg(path);
  if (temporary.write(bytes) != bytes.size() || !temporary.flush()) {
    return QStringLiteral("cannot write %1").arg(temporary.fileName());
  }
  // link() fails with EEXIST instead of replacing, which keeps the no-overwrite rule.
  if (::link(QFile::encodeName(temporary.fileName()).constData(),
             QFile::encodeName(target.absoluteFilePath()).constData()) != 0) {
    const int code = errno;
    if (code == EEXIST) return QStringLiteral("%1 already exists").arg(path);
    // No fallback to rename(): it would overwrite.
    return QStringLiteral("cannot create %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(code)));
  }
  return {};
}

}  // namespace rnqnx::probe
