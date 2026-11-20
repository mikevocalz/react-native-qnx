#include "MainThreadStack.hpp"

#include <pthread.h>
#include <sys/resource.h>

#include <cstdio>
#include <cstdlib>

namespace {
std::size_t reportedStackSize() {
  pthread_attr_t attributes;
  if (pthread_getattr_np(pthread_self(), &attributes) != 0) return 0;
  void* origin = nullptr;
  std::size_t size = 0;
  pthread_attr_getstack(&attributes, &origin, &size);
  pthread_attr_destroy(&attributes);
  return size;
}
}  // namespace

int main() {
  struct rlimit limit{};
  if (getrlimit(RLIMIT_STACK, &limit) != 0 || limit.rlim_cur == RLIM_INFINITY) {
    std::puts("MainThreadStackTest skipped: RLIMIT_STACK is unlimited");
    return EXIT_SUCCESS;
  }
  rnqnx::platform::growMainThreadStackToLimit();
  const std::size_t before = reportedStackSize();
  // Deepen the stack by 1 MiB: the reported size must not change, which is
  // what Hermes' cached bounds rely on.
  {
    volatile char deeper[1U << 20];
    for (std::size_t offset = 0; offset < sizeof deeper; offset += 4096U) deeper[offset] = 1;
    (void)deeper[0];
  }
  const std::size_t after = reportedStackSize();
  const std::size_t floor = static_cast<std::size_t>(limit.rlim_cur) * 9U / 10U;
  if (before < floor) {
    std::fprintf(stderr, "FAIL reported stack %zu is under 90%% of RLIMIT_STACK %zu\n", before,
                 static_cast<std::size_t>(limit.rlim_cur));
    return EXIT_FAILURE;
  }
  if (after != before) {
    std::fprintf(stderr, "FAIL reported stack changed from %zu to %zu\n", before, after);
    return EXIT_FAILURE;
  }
  std::printf("MainThreadStackTest passed (stack %zu, limit %zu)\n", before,
              static_cast<std::size_t>(limit.rlim_cur));
  return EXIT_SUCCESS;
}
