#pragma once

#ifndef RNQNX_ENABLE_FRAME_TRACE
#define RNQNX_ENABLE_FRAME_TRACE 0
#endif

#if RNQNX_ENABLE_FRAME_TRACE
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace rnqnx::runtime {

enum class TraceStage { Evaluate, SurfaceStart, SurfaceStop, QueueAdmission, QueueExecute, QueueClose, FrameSwapped };
enum class TracePhase { Begin, End, Instant };

struct TraceEvent {
  std::uint64_t sequence;
  std::uint64_t recordedAtNs;
  std::uint64_t correlation;
  std::int64_t surface;
  std::uint64_t surfaceGeneration;
  unsigned thread;
  TraceStage stage;
  TracePhase phase;
  bool unwinding;
};

struct TraceSnapshot {
  std::uint64_t session;
  std::uint64_t dropped;
  std::vector<TraceEvent> events;
};

/// Internal diagnostic session. No source URLs, user strings or arbitrary data.
/// Storage is allocated once; recording holds a lock only while copying a record.
/// Freeze after stopping the workload, then serialize outside callbacks/locks.
class RuntimeTrace final {
 public:
  explicit RuntimeTrace(std::size_t capacity);
  std::uint64_t nextId() noexcept;
  void record(TraceStage stage, TracePhase phase, std::uint64_t correlation,
              std::int64_t surface = 0, std::uint64_t generation = 0,
              bool unwinding = false) noexcept;
  TraceSnapshot freeze();
  static void writeJson(const TraceSnapshot& snapshot, std::ostream& output);

 private:
  const std::chrono::steady_clock::time_point origin_{std::chrono::steady_clock::now()};
  const std::uint64_t session_;
  std::mutex mutex_;
  std::vector<TraceEvent> storage_;
  std::array<std::thread::id, 64> threads_{};
  unsigned threadCount_{0};
  std::size_t count_{0};
  std::uint64_t dropped_{0};
  std::atomic<std::uint64_t> nextId_{1};
  std::atomic<std::uint64_t> recorderFailures_{0};
  bool frozen_{false};
};

/// An end marker means this native boundary returned/unwound, not success,
/// a mounted frame, compositor presentation or a physical display observation.
class TraceSpan final {
 public:
  TraceSpan(std::shared_ptr<RuntimeTrace> trace, TraceStage stage,
            std::int64_t surface = 0, std::uint64_t generation = 0);
  ~TraceSpan();
  TraceSpan(const TraceSpan&) = delete;
  TraceSpan& operator=(const TraceSpan&) = delete;

 private:
  std::shared_ptr<RuntimeTrace> trace_;
  TraceStage stage_;
  std::int64_t surface_;
  std::uint64_t generation_;
  std::uint64_t correlation_{0};
  int exceptions_;
};
} // namespace rnqnx::runtime
#endif
