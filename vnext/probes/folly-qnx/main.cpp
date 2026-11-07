#include <folly/net/NetOps.h>
#include <folly/portability/SysSyscall.h>
#include <folly/system/ThreadId.h>

#include <pthread.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {
struct SocketPair {
  std::array<int, 2> descriptors{-1, -1};
  SocketPair() = default;
  SocketPair(const SocketPair&) = delete;
  SocketPair& operator=(const SocketPair&) = delete;
  ~SocketPair() {
    for (const int descriptor : descriptors) {
      if (descriptor >= 0) {
        close(descriptor);
      }
    }
  }
};

void stage(const char* message) {
  std::puts(message);
  std::fflush(stdout);
  // Persist progress when stdout is redirected to a target-side log.
  static_cast<void>(fsync(fileno(stdout)));
}
}  // namespace

int main(int argc, char** argv) {
  const bool threadsOnly = argc == 2 && std::strcmp(argv[1], "--threads-only") == 0;
  if (argc > 1 && !threadsOnly) {
    std::fprintf(stderr, "Usage: rnqnx_folly_qnx_probe [--threads-only]\n");
    return 2;
  }
  stage("RNQNX_FOLLY_STAGE main");
  const auto mainId = folly::getOSThreadID();
  std::uint64_t workerId = 0;
  bool workerMatchesNative = false;
  std::thread worker([&]() {
    workerId = folly::getOSThreadID();
    workerMatchesNative = workerId == std::uint64_t(pthread_self());
  });
  worker.join();
  stage("RNQNX_FOLLY_STAGE worker-joined");
  if (mainId != std::uint64_t(pthread_self()) || !workerMatchesNative ||
      mainId == 0 || workerId == 0 || mainId == workerId) {
    std::fprintf(stderr, "Folly native QNX thread IDs failed\n");
    return 1;
  }

  errno = 0;
  if (folly::detail::linux_syscall(-1L) != -1 || errno != ENOSYS) {
    std::fprintf(stderr, "Unsupported Linux syscall did not report ENOSYS\n");
    return 1;
  }
  if (threadsOnly) {
    std::puts("RNQNX_FOLLY_QNX_OK threads=2 syscall=ENOSYS batch=not-run");
    return 0;
  }

  stage("RNQNX_FOLLY_STAGE socketpair");
  SocketPair sockets;
  if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets.descriptors.data()) != 0) {
    std::perror("socketpair");
    return 1;
  }
  std::array<char, 2> sent{'a', 'b'};
  std::array<char, 2> received{};
  std::array<iovec, 2> sendBuffers{};
  std::array<iovec, 2> receiveBuffers{};
  std::array<mmsghdr, 2> sendMessages{};
  std::array<mmsghdr, 2> receiveMessages{};
  for (std::size_t i = 0; i < sent.size(); ++i) {
    sendBuffers[i] = {&sent[i], 1};
    receiveBuffers[i] = {&received[i], 1};
    sendMessages[i].msg_hdr.msg_iov = &sendBuffers[i];
    sendMessages[i].msg_hdr.msg_iovlen = 1;
    receiveMessages[i].msg_hdr.msg_iov = &receiveBuffers[i];
    receiveMessages[i].msg_hdr.msg_iovlen = 1;
  }
  stage("RNQNX_FOLLY_STAGE send-batch");
  const int sentCount = folly::netops::sendmmsg(
      folly::NetworkSocket::fromFd(sockets.descriptors[0]),
      sendMessages.data(), 2, 0);
  timespec timeout{2, 0};
  stage("RNQNX_FOLLY_STAGE receive-batch");
  const int receivedCount = folly::netops::recvmmsg(
      folly::NetworkSocket::fromFd(sockets.descriptors[1]),
      receiveMessages.data(), 2, MSG_DONTWAIT, &timeout);
  if (sentCount != 2 || receivedCount != 2 || received != sent ||
      receiveMessages[0].msg_len != 1 || receiveMessages[1].msg_len != 1) {
    std::fprintf(stderr, "Folly native QNX socket batch failed: sent=%d recv=%d errno=%d\n",
                 sentCount, receivedCount, errno);
    return 1;
  }
  std::puts("RNQNX_FOLLY_QNX_OK threads=2 batch=2 syscall=ENOSYS");
  return 0;
}
