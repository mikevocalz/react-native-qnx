#include "QnxTimingSource.h"

#include "QnxProcessMemory.h"

#if defined(__QNX__)
#include <sys/neutrino.h>
#include <sys/syspage.h>
#endif

namespace rnqnx::probe {

#if defined(__QNX__)
namespace {

// ClockCycles() is the QNX free-running cycle counter and
// SYSPAGE_ENTRY(qtime)->cycles_per_sec is its rate. The resident set comes from
// procfs page data (QnxProcessMemory.h). QNX reports no peak, so peakResidentKiB
// is the highest value residentKiB() has returned: call residentKiB() while the
// workload runs, not only at the end.
class CycleCounterSource final : public QnxTimingSource {
 public:
  std::uint64_t nowTicks() const noexcept override { return ClockCycles(); }
  std::uint64_t ticksPerSecond() const noexcept override {
    return SYSPAGE_ENTRY(qtime)->cycles_per_sec;
  }
  std::optional<std::uint64_t> residentKiB() const noexcept override {
    const auto kib = readOwnResidentKiB();
    if (kib) peak_.record(*kib);
    return kib;
  }
  std::optional<std::uint64_t> peakResidentKiB() const noexcept override { return peak_.peak(); }

 private:
  mutable PeakTracker peak_;
};

}  // namespace

std::unique_ptr<QnxTimingSource> makeQnxTimingSource() {
  return std::make_unique<CycleCounterSource>();
}
#else
std::unique_ptr<QnxTimingSource> makeQnxTimingSource() { return nullptr; }
#endif

}  // namespace rnqnx::probe
