#include "QnxTimingSource.h"

#if defined(__QNX__)
#include <sys/neutrino.h>
#include <sys/syspage.h>
#endif

namespace rnqnx::probe {

#if defined(__QNX__)
namespace {

// ClockCycles() is the QNX free-running cycle counter and
// SYSPAGE_ENTRY(qtime)->cycles_per_sec is its rate. Memory readers are left
// unavailable until a procfs reader has been built and checked on a target.
class CycleCounterSource final : public QnxTimingSource {
 public:
  std::uint64_t nowTicks() const noexcept override { return ClockCycles(); }
  std::uint64_t ticksPerSecond() const noexcept override {
    return SYSPAGE_ENTRY(qtime)->cycles_per_sec;
  }
  std::optional<std::uint64_t> residentKiB() const noexcept override { return std::nullopt; }
  std::optional<std::uint64_t> peakResidentKiB() const noexcept override { return std::nullopt; }
};

}  // namespace

std::unique_ptr<QnxTimingSource> makeQnxTimingSource() {
  return std::make_unique<CycleCounterSource>();
}
#else
std::unique_ptr<QnxTimingSource> makeQnxTimingSource() { return nullptr; }
#endif

}  // namespace rnqnx::probe
