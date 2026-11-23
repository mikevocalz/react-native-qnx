// Host test for vnext/probes/timing/TimingReader.h. Label: host. It says nothing about QNX.
#include "TimingReader.h"

#include <cstdint>
#include <cstdio>
#include <vector>

using namespace rnqnx::probe;

namespace {
int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "%s:%d CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

constexpr std::int64_t kMs = 1'000'000;

std::vector<std::int64_t> steady(int count, std::int64_t startNs, std::int64_t gapNs) {
  std::vector<std::int64_t> v;
  for (int i = 0; i < count; ++i) v.push_back(startNs + i * gapNs);
  return v;
}
}  // namespace

int main() {
  // TickClock rejects an unreadable or absurd rate.
  CHECK(!TickClock::make(0));
  CHECK(!TickClock::make((1ULL << 32) + 1));
  CHECK(TickClock::make(1ULL << 32));

  // Exact conversions.
  const auto ghz = *TickClock::make(1'000'000'000);
  CHECK(*ghz.toNanoseconds(0) == 0);
  CHECK(*ghz.toNanoseconds(123456789) == 123456789);
  const auto mhz24 = *TickClock::make(24'000'000);
  CHECK(*mhz24.toNanoseconds(24'000'000) == 1'000'000'000);
  CHECK(*mhz24.toNanoseconds(1) == 41);  // 41.666 ns rounds down

  // No overflow where a naive ticks * 1e9 wraps: 100 s at 1 GHz is 1e11 ticks.
  CHECK(*ghz.toNanoseconds(100'000'000'000ULL) == 100'000'000'000LL);
  // 2^63 ns is the largest signed count; one tick past it is refused.
  const auto one = *TickClock::make(1);
  CHECK(!one.toNanoseconds(9'223'372'037ULL));  // 9.223372037e9 s * 1e9 > INT64_MAX
  CHECK(one.toNanoseconds(9'223'372'036ULL).has_value());
  const auto rate4g = *TickClock::make(1ULL << 32);
  CHECK(rate4g.toNanoseconds(1ULL << 32).value() == 1'000'000'000);
  CHECK(rate4g.toNanoseconds((1ULL << 32) - 1).value() == 999'999'999);

  // Mapping to the origin; a reading before it is refused, not clamped.
  const ClockMapping map(ghz, 5'000);
  CHECK(*map.sinceOrigin(5'000) == 0);
  CHECK(*map.sinceOrigin(5'000 + 101'299'000) == 101'299'000);
  CHECK(!map.sinceOrigin(4'999));

  // nearestRank matches percentile() in validate-budgets.mjs.
  std::vector<std::int64_t> ten{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  CHECK(*nearestRank(ten, 95) == 10);  // ceil(9.5) = 10
  CHECK(*nearestRank(ten, 50) == 5);
  CHECK(*nearestRank(ten, 1) == 1);
  CHECK(!nearestRank({}, 95));
  CHECK(!nearestRank(ten, 0));
  CHECK(!nearestRank(ten, 101));
  std::vector<std::int64_t> twenty;
  for (int i = 1; i <= 20; ++i) twenty.push_back(i);
  CHECK(*nearestRank(twenty, 95) == 19);  // ceil(19.0) = 19

  // 40 swaps 16 ms apart inside the window; one 40 ms stall.
  auto swaps = steady(40, 1000 * kMs, 16 * kMs);
  for (std::size_t i = 20; i < swaps.size(); ++i) swaps[i] += 24 * kMs;
  const FrameIntervals ok = frameIntervals(swaps, swaps.front(), swaps.back());
  CHECK(ok.error == WindowError::None);
  CHECK(ok.swapsInWindow == 40);
  CHECK(ok.maxNs == 40 * kMs);
  CHECK(ok.p95Ns == 16 * kMs);  // 39 gaps, rank ceil(37.05) = 38, one stall above it

  // Swaps outside the window are excluded, bounds are inclusive.
  auto padded = steady(5, 0, 16 * kMs);
  auto body = steady(30, 500 * kMs, 16 * kMs);
  padded.insert(padded.end(), body.begin(), body.end());
  auto tail = steady(5, 2000 * kMs, 16 * kMs);
  padded.insert(padded.end(), tail.begin(), tail.end());
  const FrameIntervals bounded = frameIntervals(padded, body.front(), body.back());
  CHECK(bounded.error == WindowError::None);
  CHECK(bounded.swapsInWindow == 30);

  // Rejections.
  const auto few = steady(29, 500 * kMs, 16 * kMs);
  CHECK(frameIntervals(few, few.front(), few.back()).error == WindowError::TooFewSwaps);
  auto backwards = steady(40, 500 * kMs, 16 * kMs);
  std::swap(backwards[10], backwards[11]);
  CHECK(frameIntervals(backwards, 0, 9999 * kMs).error == WindowError::NotAscending);
  CHECK(frameIntervals(swaps, 10, 5).error == WindowError::NoWheelWindow);
  CHECK(frameIntervals(swaps, -1, 5).error == WindowError::NoWheelWindow);

  if (failures == 0) std::puts("timing reader: all checks passed");
  return failures == 0 ? 0 : 1;
}
