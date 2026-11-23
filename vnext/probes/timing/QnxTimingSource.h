#pragma once

// Raw counter source for a QNX run, behind an interface so the clock mapping in
// TimingReader.h is tested on the host with a fake.
//
// STATUS: written, not built and not run on QNX. No QNX measurement exists.
// The QNX implementation (QnxTimingSource.cpp) is guarded by __QNX__, is not in
// any CMake target, and needs a first build and a run on each target before any
// qnx-vm, pi400 or release-board budget can leave `unmeasured`. The Qt
// frameSwapped hook on QNX is also unwritten: TimingProbe.cpp stays host-only
// (CMake rejects RNQNX_ENABLE_TIMING_PROBE on QNX).

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
