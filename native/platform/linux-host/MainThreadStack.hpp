#pragma once

// musl reports the main thread's stack as the part of it that is mapped right
// now (pthread_getattr_np reads the stack VMA from /proc/self/maps), which is
// about 132 KiB at process start and grows as the stack deepens. glibc reports
// RLIMIT_STACK. Hermes asks once, caches the answer, and treats the cached
// size as the whole native stack
// (hermes/lib/Support/OSCompatCommon.cpp thread_stack_bounds). On musl that
// made every JavaScript call made from the Qt event loop fail with "Maximum
// call stack size exceeded (native stack depth)" as soon as React rendering
// went deeper than the startup stack, and made Hermes' debug check
// `bounds == thread_stack_bounds_impl()` abort.
//
// Touching the stack down to the limit before Hermes first asks makes the
// mapped extent equal what the kernel will allow, so the cached value is right.

#include <pthread.h>
#include <sys/resource.h>

#include <cstddef>
#include <cstdint>

namespace rnqnx::platform {

#if defined(__linux__) && !defined(__GLIBC__)
// Not inlined: the frame must be the one holding the alloca.
__attribute__((noinline)) inline void growMainThreadStackToLimit() {
  struct rlimit limit{};
  if (getrlimit(RLIMIT_STACK, &limit) != 0 || limit.rlim_cur == RLIM_INFINITY) return;
  // RLIMIT_STACK counts from the top of the stack, which already holds the
  // arguments, environment and auxiliary vector, so measure what is used.
  pthread_attr_t attributes;
  if (pthread_getattr_np(pthread_self(), &attributes) != 0) return;
  void* low = nullptr;
  std::size_t mapped = 0;
  const int status = pthread_attr_getstack(&attributes, &low, &mapped);
  pthread_attr_destroy(&attributes);
  if (status != 0) return;
  const auto top = reinterpret_cast<std::uintptr_t>(low) + mapped;
  const auto here = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
  if (top <= here) return;
  const std::size_t used = static_cast<std::size_t>(top - here);
  // Leave room for this call chain, signal frames and the kernel's stack guard gap.
  constexpr std::size_t kReserve = 512U * 1024U;
  constexpr std::size_t kPage = 4096U;
  if (limit.rlim_cur <= used + 2 * kReserve) return;
  const std::size_t bytes = static_cast<std::size_t>(limit.rlim_cur) - used - kReserve;
  // alloca, not a VLA or recursion: sanitizer builds keep alloca on the real
  // stack, whereas large locals can move to a heap-backed fake stack.
  volatile char* block = static_cast<volatile char*>(__builtin_alloca(bytes));
  for (std::size_t offset = 0; offset < bytes; offset += kPage) block[offset] = 0;
}
#else
inline void growMainThreadStackToLimit() {}
#endif

}  // namespace rnqnx::platform
