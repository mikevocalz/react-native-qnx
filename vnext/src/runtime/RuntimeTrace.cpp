#include "react-native-qnx/runtime/RuntimeTrace.h"

#if RNQNX_ENABLE_FRAME_TRACE
#include <atomic>
#include <exception>
#include <ostream>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace rnqnx::runtime {
namespace {
std::atomic<std::uint64_t> nextSession{1};
const char* stageName(TraceStage stage) {
  switch (stage) {
    case TraceStage::Evaluate: return "host.evaluate";
    case TraceStage::SurfaceStart: return "surface.start";
    case TraceStage::SurfaceStop: return "surface.stop";
    case TraceStage::QueueAdmission: return "queue.admit";
    case TraceStage::QueueExecute: return "queue.execute";
    case TraceStage::QueueClose: return "queue.close";
    case TraceStage::FrameQueued: return "qt.frame.queued";
  }
  return "unknown";
}
const char* phaseName(TracePhase phase) {
  switch (phase) {
    case TracePhase::Begin: return "begin";
    case TracePhase::End: return "end";
    case TracePhase::Instant: return "instant";
  }
  return "unknown";
}
} // namespace

RuntimeTrace::RuntimeTrace(std::size_t capacity)
    : session_(nextSession.fetch_add(1, std::memory_order_relaxed)) {
  if (capacity == 0 || capacity > 65536) throw std::invalid_argument("trace capacity must be 1..65536");
  storage_.resize(capacity);
}

std::uint64_t RuntimeTrace::nextId() noexcept {
  return nextId_.fetch_add(1, std::memory_order_relaxed);
}

void RuntimeTrace::record(TraceStage stage, TracePhase phase, std::uint64_t correlation,
                          std::int64_t surface, std::uint64_t generation, bool unwinding) noexcept {
  // No application code or clock adapter is invoked under this recorder lock.
  // Platform mutex failure must not throw through an application callback.
  try {
    std::lock_guard lock(mutex_);
    if (frozen_) return;
    if (count_ == storage_.size()) {
      ++dropped_;
      return;
    }
    const auto identity = std::this_thread::get_id();
    unsigned thread = 0;
    while (thread < threadCount_ && threads_[thread] != identity) ++thread;
    if (thread == threadCount_) {
      if (threadCount_ == threads_.size()) {
        ++dropped_;
        return;
      }
      threads_[threadCount_++] = identity;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - origin_).count();
    storage_[count_] = TraceEvent{count_ + 1, static_cast<std::uint64_t>(elapsed), correlation,
                                 surface, generation, thread + 1, stage, phase, unwinding};
    ++count_;
  } catch (...) {
    recorderFailures_.fetch_add(1, std::memory_order_relaxed);
  }
}

TraceSnapshot RuntimeTrace::freeze() {
  std::lock_guard lock(mutex_);
  frozen_ = true;
  return TraceSnapshot{session_, dropped_ + recorderFailures_.load(std::memory_order_relaxed),
      std::vector<TraceEvent>(storage_.begin(), storage_.begin() + static_cast<std::ptrdiff_t>(count_))};
}

void RuntimeTrace::writeJson(const TraceSnapshot& snapshot, std::ostream& output) {
  // Formatting is explicit export work, outside recorder locks and callbacks.
  // Preserve JSON numbers even when the caller imbues a grouping locale.
  std::ostringstream encoded;
  encoded.imbue(std::locale::classic());
  encoded << "{\"schemaVersion\":1,\"clock\":\"std::chrono::steady_clock\","
         << "\"clockScope\":\"session-relative; not aligned to Qt/QNX/display clocks\","
         << "\"frameBoundaryMeaning\":\"qt.frame.queued is submission notification, not display presentation\","
         << "\"unit\":\"nanoseconds\",\"timestampMeaning\":\"record-copy after recorder lock\","
         << "\"threadIdentity\":\"session-local ordinal of std::thread::id; IDs may be reused\","
         << "\"session\":" << snapshot.session << ",\"dropped\":" << snapshot.dropped << ",\"events\":[";
  bool first = true;
  for (const auto& event : snapshot.events) {
    if (!first) encoded << ',';
    first = false;
    encoded << "{\"sequence\":" << event.sequence << ",\"recordedAtNs\":" << event.recordedAtNs
           << ",\"thread\":" << event.thread << ",\"surface\":" << event.surface
           << ",\"surfaceGeneration\":" << event.surfaceGeneration << ",\"correlation\":" << event.correlation
           << ",\"stage\":\"" << stageName(event.stage) << "\",\"phase\":\"" << phaseName(event.phase)
           << "\",\"unwinding\":" << (event.unwinding ? "true" : "false") << '}';
  }
  encoded << "]}\n";
  output << encoded.str();
  if (!output) throw std::runtime_error("trace export failed");
}

TraceSpan::TraceSpan(std::shared_ptr<RuntimeTrace> trace, TraceStage stage,
                     std::int64_t surface, std::uint64_t generation)
    : trace_(std::move(trace)), stage_(stage), surface_(surface), generation_(generation),
      exceptions_(std::uncaught_exceptions()) {
  if (trace_) {
    correlation_ = trace_->nextId();
    trace_->record(stage_, TracePhase::Begin, correlation_, surface_, generation_);
  }
}
TraceSpan::~TraceSpan() {
  if (trace_) trace_->record(stage_, TracePhase::End, correlation_, surface_, generation_,
                             std::uncaught_exceptions() > exceptions_);
}
} // namespace rnqnx::runtime
#endif
