#pragma once

// Platform-neutral timing math for the performance gate: tick-to-nanosecond
// clock mapping and frame-interval statistics. No Qt, no I/O, no allocation
// beyond the vectors it returns, so it builds and tests on the host.
//
// The statistics repeat the rules in scripts/validate-budgets.mjs (summarizeRun):
// a window is the swaps between the first and last wheel notch, at least 30
// swaps must fall in it, and p95 is nearest-rank over the sorted gaps.
// A swap is the end of a scene-graph swap, not a display presentation.
//
// Host tests only. Nothing here has run on QNX; see QnxTimingSource.h.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace rnqnx::probe {

inline constexpr std::size_t kMinWindowSwaps = 30;

// Converts a free-running counter that ticks `ticksPerSecond` times a second
// into nanoseconds. Splitting into whole seconds and a remainder keeps every
// intermediate below 2^64 for any counter value and any rate up to 2^32 Hz,
// where a plain `ticks * 1e9 / rate` overflows after about 18 s at 1 GHz.
class TickClock {
 public:
  static std::optional<TickClock> make(std::uint64_t ticksPerSecond) {
    if (ticksPerSecond == 0 || ticksPerSecond > kMaxRate) return std::nullopt;
    return TickClock(ticksPerSecond);
  }

  std::uint64_t ticksPerSecond() const noexcept { return rate_; }

  // Nanoseconds for `ticks`, rounded down, or nullopt when the result does not
  // fit a signed 64-bit count (about 292 years).
  std::optional<std::int64_t> toNanoseconds(std::uint64_t ticks) const noexcept {
    const std::uint64_t seconds = ticks / rate_;
    const std::uint64_t rest = ticks % rate_;
    constexpr std::uint64_t kNs = 1'000'000'000ULL;
    if (seconds > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / kNs) {
      return std::nullopt;
    }
    // rest < rate_ <= 2^32, so rest * kNs < 2^32 * 1e9 < 2^62.
    const std::uint64_t restNs = rest * kNs / rate_;
    const std::uint64_t total = seconds * kNs + restNs;
    if (total > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      return std::nullopt;
    }
    return static_cast<std::int64_t>(total);
  }

 private:
  static constexpr std::uint64_t kMaxRate = 1ULL << 32;
  explicit TickClock(std::uint64_t rate) : rate_(rate) {}
  std::uint64_t rate_;
};

// Maps raw counter readings to nanoseconds since the reading taken as the
// first statement of main(). A reading earlier than the origin means the
// counter is not monotonic for this process (a different clock, a wrap or a
// migration between cores with unsynchronised counters); it is reported, not
// clamped to zero.
class ClockMapping {
 public:
  ClockMapping(TickClock clock, std::uint64_t originTicks) noexcept
      : clock_(clock), origin_(originTicks) {}

  std::optional<std::int64_t> sinceOrigin(std::uint64_t ticks) const noexcept {
    if (ticks < origin_) return std::nullopt;
    return clock_.toNanoseconds(ticks - origin_);
  }

 private:
  TickClock clock_;
  std::uint64_t origin_;
};

// Nearest-rank percentile of an ascending range, as percentile() in
// scripts/validate-budgets.mjs. `p` is 1 to 100.
inline std::optional<std::int64_t> nearestRank(const std::vector<std::int64_t>& sorted, int p) {
  if (sorted.empty() || p < 1 || p > 100) return std::nullopt;
  const std::size_t n = sorted.size();
  const std::size_t rank = std::max<std::size_t>(1, (static_cast<std::size_t>(p) * n + 99) / 100);
  return sorted[rank - 1];
}

enum class WindowError {
  None,
  NotAscending,       // swap instants must not go backwards
  NoWheelWindow,      // wheel bounds missing or reversed
  TooFewSwaps,        // fewer than kMinWindowSwaps inside the window
};

struct FrameIntervals {
  WindowError error = WindowError::None;
  std::size_t swapsInWindow = 0;
  std::int64_t p95Ns = 0;
  std::int64_t maxNs = 0;
};

// `swapNs` are swap instants in nanoseconds since the origin, ascending.
// `wheelFirstNs` and `wheelLastNs` bound the window, both inclusive.
inline FrameIntervals frameIntervals(const std::vector<std::int64_t>& swapNs,
                                     std::int64_t wheelFirstNs, std::int64_t wheelLastNs) {
  FrameIntervals out;
  if (!std::is_sorted(swapNs.begin(), swapNs.end())) {
    out.error = WindowError::NotAscending;
    return out;
  }
  if (wheelFirstNs < 0 || wheelLastNs < wheelFirstNs) {
    out.error = WindowError::NoWheelWindow;
    return out;
  }
  const auto first = std::lower_bound(swapNs.begin(), swapNs.end(), wheelFirstNs);
  const auto last = std::upper_bound(swapNs.begin(), swapNs.end(), wheelLastNs);
  out.swapsInWindow = static_cast<std::size_t>(last - first);
  if (out.swapsInWindow < kMinWindowSwaps) {
    out.error = WindowError::TooFewSwaps;
    return out;
  }
  std::vector<std::int64_t> gaps;
  gaps.reserve(out.swapsInWindow - 1);
  for (auto it = first + 1; it != last; ++it) gaps.push_back(*it - *(it - 1));
  std::sort(gaps.begin(), gaps.end());
  out.p95Ns = *nearestRank(gaps, 95);
  out.maxNs = gaps.back();
  return out;
}

}  // namespace rnqnx::probe
