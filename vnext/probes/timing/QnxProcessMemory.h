#pragma once

// Resident-set reader for a QNX process, from the process page data that procfs
// serves (DCMD_PROC_PAGEDATA on /proc/<pid>/as). QNX has no VmRSS or VmHWM and
// getrusage() reports ru_maxrss 0 (read on a Pi 400 2026-11-23), so:
//   - the resident size is the sum of the page runs the kernel marks as mapped
//     in hardware (PG_HWMAPPED, sys/mman.h), and
//   - a peak is the highest value over the samples a caller took. It is a lower
//     bound on the true peak, never an OS-reported one. PeakTracker does the
//     bookkeeping and the JSON carries rssSource "sampled-procfs".
//
// The parsing and the peak bookkeeping are plain C++ and run on the host. The
// devctl reader exists on QNX only.

#include <cstdint>
#include <optional>
#include <vector>

namespace rnqnx::probe {

// One run of pages as DCMD_PROC_PAGEDATA returns it (procfs_mapinfo: vaddr,
// size in bytes, flags). Only what the sum needs.
struct PageRun {
  std::uint64_t sizeBytes = 0;
  std::uint32_t flags = 0;
};

// PG_HWMAPPED in sys/mman.h of QNX SDP 8.0.5: pages present and mapped.
inline constexpr std::uint32_t kPageHwMapped = 0x00400000;

// KiB held by runs whose flags contain `residentMask`, rounded down. Returns
// nullopt when the sum does not fit 64 bits, which only a corrupt reply can cause.
std::optional<std::uint64_t> residentKiB(const std::vector<PageRun>& runs,
                                         std::uint32_t residentMask = kPageHwMapped);

// Highest sample seen. Not thread-safe: one owner thread.
class PeakTracker {
 public:
  void record(std::uint64_t kib) noexcept {
    if (!peak_ || kib > *peak_) peak_ = kib;
  }
  std::optional<std::uint64_t> peak() const noexcept { return peak_; }

 private:
  std::optional<std::uint64_t> peak_;
};

// Reads the calling process. Defined on QNX only; returns nullopt elsewhere and
// on any devctl failure. Nothing is estimated.
std::optional<std::uint64_t> readOwnResidentKiB();

}  // namespace rnqnx::probe
