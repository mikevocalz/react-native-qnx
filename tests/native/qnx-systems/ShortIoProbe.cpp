#include "Probe.h"

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace rnqnx::systems {
namespace {
class Pipe final {
 public:
  Pipe() {
    if (pipe(descriptors_.data()) != 0) throw std::runtime_error("pipe-create");
  }
  ~Pipe() {
    for (int descriptor : descriptors_) {
      if (descriptor >= 0) close(descriptor);
    }
  }
  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
  int readEnd() const { return descriptors_[0]; }
  int writeEnd() const { return descriptors_[1]; }
  void closeWriter() {
    const int descriptor = descriptors_[1];
    // Do not retry close after EINTR: ownership may already have ended.
    descriptors_[1] = -1;
    if (close(descriptor) != 0) throw std::runtime_error("pipe-close-writer");
    errno = 0;
    if (fcntl(descriptor, F_GETFD) != -1 || errno != EBADF) {
      throw std::runtime_error("pipe-writer-leaked");
    }
  }
  void closeReader() {
    const int descriptor = descriptors_[0];
    descriptors_[0] = -1;
    if (close(descriptor) != 0) throw std::runtime_error("pipe-close-reader");
  }

 private:
  std::array<int, 2> descriptors_{};
};

void nonblocking(int descriptor) {
  const int flags = fcntl(descriptor, F_GETFL);
  if (flags == -1 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == -1) {
    throw std::runtime_error("pipe-nonblocking");
  }
}
} // namespace

void probeShortIo(bool injectEarlyEof) {
  Pipe stream;
  nonblocking(stream.readEnd());
  nonblocking(stream.writeEnd());
  std::array<char, 64> received{};
  errno = 0;
  if (read(stream.readEnd(), received.data(), received.size()) != -1 ||
      (errno != EAGAIN && errno != EWOULDBLOCK)) {
    throw std::runtime_error("pipe-empty-admission");
  }
  constexpr std::string_view payload = "rnqnx-short-io";
  const auto committed = write(stream.writeEnd(), payload.data(), payload.size());
  if (committed < 0 || static_cast<std::size_t>(committed) != payload.size()) {
    throw std::runtime_error("pipe-write-count");
  }
  stream.closeWriter();
  const auto count = read(stream.readEnd(), received.data(), received.size());
  if (count <= 0 || static_cast<std::size_t>(count) != payload.size() ||
      count >= static_cast<ssize_t>(received.size()) ||
      std::string_view(received.data(), static_cast<std::size_t>(count)) != payload) {
    throw std::runtime_error("pipe-short-read-count");
  }
  if (read(stream.readEnd(), received.data(), received.size()) != 0) {
    throw std::runtime_error("pipe-eof");
  }
  // A consumer expecting one more byte must observe and reject the real EOF.
  if (injectEarlyEof && read(stream.readEnd(), received.data(), 1) == 0) {
    throw std::runtime_error("unexpected-eof");
  }
  // Preserve the closed descriptor number for an immediate cleanup assertion;
  // this single-threaded fixture opens no other descriptor in between.
  const int closed = stream.readEnd();
  stream.closeReader();
  errno = 0;
  if (fcntl(closed, F_GETFD) != -1 || errno != EBADF) {
    throw std::runtime_error("pipe-reader-leaked");
  }

}
} // namespace rnqnx::systems
