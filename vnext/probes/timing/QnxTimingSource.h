#pragma once

// Raw counter source for a QNX run, behind an interface so the clock mapping in
// TimingReader.h is tested on the host with a fake.
//
// STATUS: the memory reader (QnxProcessMemory.cpp) was cross-compiled with
// SDP 8.0.5 and run on a Pi 400 on 2026-11-23, see
// docs/evidence/pi400-timing-reader-2026-11-23. The cycle-counter part of
// QnxTimingSource.cpp has not been built or run: the playground probe times with
// steady_clock, so nothing links this class yet. No pi400 or qnx-vm budget has
// left `unmeasured`.
//
// QNX reports no peak resident size. peakResidentKiB() is the highest value
// residentKiB() returned, a lower bound on the true peak.

#include <cstdint>
#include <memory>
#include <optional>

namespace rnqnx::probe {

class QnxTimingSource {
 public:
  virtual ~QnxTimingSource() = default;
  // Free-running counter reading, in ticks of ticksPerSecond().
  virtual std::uint64_t nowTicks() const noexcept = 0;
  // Counter rate, or 0 when it cannot be read (the reader then refuses to run).
  virtual std::uint64_t ticksPerSecond() const noexcept = 0;
  // Resident set in KiB, or nullopt. Nothing is estimated.
  virtual std::optional<std::uint64_t> residentKiB() const noexcept = 0;
  // Peak resident set in KiB, or nullopt.
  virtual std::optional<std::uint64_t> peakResidentKiB() const noexcept = 0;
};

// Defined in QnxTimingSource.cpp on QNX only. Returns null elsewhere.
std::unique_ptr<QnxTimingSource> makeQnxTimingSource();

}  // namespace rnqnx::probe
