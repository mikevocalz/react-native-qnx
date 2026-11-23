#include "QnxProcessMemory.h"

#include <limits>

#if defined(__QNX__)
#include <devctl.h>
#include <fcntl.h>
#include <sys/procfs.h>
#include <unistd.h>

#include <cstddef>
#endif

namespace rnqnx::probe {

std::optional<std::uint64_t> residentKiB(const std::vector<PageRun>& runs,
                                         std::uint32_t residentMask) {
  std::uint64_t bytes = 0;
  for (const PageRun& run : runs) {
    if ((run.flags & residentMask) != residentMask) continue;
    if (run.sizeBytes > std::numeric_limits<std::uint64_t>::max() - bytes) return std::nullopt;
    bytes += run.sizeBytes;
  }
  return bytes / 1024;
}

#if defined(__QNX__)
namespace {

class Fd {
 public:
  explicit Fd(int fd) noexcept : fd_(fd) {}
  ~Fd() {
    if (fd_ >= 0) ::close(fd_);
  }
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;
  int get() const noexcept { return fd_; }

 private:
  int fd_;
};

}  // namespace

std::optional<std::uint64_t> readOwnResidentKiB() {
  const Fd fd(::open("/proc/self/as", O_RDONLY));
  if (fd.get() < 0) return std::nullopt;
  // The address space can grow between the count and the fetch, so ask for room
  // beyond the count and retry when the kernel reports more entries than fit.
  int count = 0;
  if (::devctl(fd.get(), DCMD_PROC_PAGEDATA, nullptr, 0, &count) != EOK || count < 0) {
    return std::nullopt;
  }
  for (int attempt = 0; attempt < 4; ++attempt) {
    const std::size_t capacity = static_cast<std::size_t>(count) + 64;
    std::vector<procfs_mapinfo> entries(capacity);
    int returned = 0;
    if (::devctl(fd.get(), DCMD_PROC_PAGEDATA, entries.data(),
                 static_cast<int>(capacity * sizeof(procfs_mapinfo)), &returned) != EOK ||
        returned < 0) {
      return std::nullopt;
    }
    if (static_cast<std::size_t>(returned) > capacity) {
      count = returned;
      continue;
    }
    std::vector<PageRun> runs;
    runs.reserve(static_cast<std::size_t>(returned));
    for (int i = 0; i < returned; ++i) {
      runs.push_back({entries[static_cast<std::size_t>(i)].size,
                      entries[static_cast<std::size_t>(i)].flags});
    }
    return residentKiB(runs);
  }
  return std::nullopt;
}
#else
std::optional<std::uint64_t> readOwnResidentKiB() { return std::nullopt; }
#endif

}  // namespace rnqnx::probe
