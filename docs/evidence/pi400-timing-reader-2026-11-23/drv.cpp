#include "QnxTimingSource.h"
#include "QnxProcessMemory.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
int main(int argc, char** argv) {
  using namespace rnqnx::probe;
  auto src = makeQnxTimingSource();
  if (!src) { std::puts("no source"); return 1; }
  std::printf("cycles_per_sec=%llu\n", (unsigned long long)src->ticksPerSecond());
  auto a = src->residentKiB();
  std::printf("rss_before=%llu KiB\n", a ? (unsigned long long)*a : 0ULL);
  size_t mb = argc > 1 ? std::atoi(argv[1]) : 100;
  volatile char* p = static_cast<char*>(std::malloc(mb << 20));
  for (size_t i = 0; i < (mb << 20); i += 4096) p[i] = 1;
  auto b = src->residentKiB();
  std::printf("rss_after_%zuMiB=%llu KiB delta=%lld KiB\n", mb, b ? (unsigned long long)*b : 0ULL, (long long)(b && a ? (long long)*b - (long long)*a : -1));
  std::free((void*)p);
  auto c = src->residentKiB();
  std::printf("rss_after_free=%llu KiB peak=%llu KiB\n", c ? (unsigned long long)*c : 0ULL, (unsigned long long)*src->peakResidentKiB());
  std::printf("pid=%d\n", (int)getpid());
  sleep(argc > 2 ? std::atoi(argv[2]) : 0);
  return 0;
}
