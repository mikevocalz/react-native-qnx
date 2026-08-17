#include <array>
#include <atomic>
#include <concepts>
#include <cstdio>
#include <ranges>
#include <thread>

namespace {

template <std::integral Value>
constexpr Value doubled(Value value) {
  return value * 2;
}

struct ProbeResult final {
  int rangeTotal;
  int designatedValue;
  int threadValue;
};

} // namespace

int main() {
  constexpr std::array values{1, 2, 3, 4, 5, 6};
  constexpr auto isEven = [](int value) { return value % 2 == 0; };

  int rangeTotal = 0;
  for (const int value : values | std::views::filter(isEven)) {
    rangeTotal += value;
  }

  std::atomic<int> threadValue{0};

  // QNX SDP 8.0's libc++ disables experimental stop_token/jthread, so fall
  // back to std::thread on targets where std::jthread is not in the library.
#if defined(__cpp_lib_jthread) && (__cpp_lib_jthread >= 201911L)
  std::jthread worker([&threadValue] { threadValue.store(34); });
  worker.join();
  const char* threadBackend = "jthread=34";
#else
  std::thread worker([&threadValue] { threadValue.store(34); });
  worker.join();
  const char* threadBackend = "thread=34 jthread=unsupported";
#endif

  const ProbeResult result{
      .rangeTotal = rangeTotal,
      .designatedValue = doubled(10) + 1,
      .threadValue = threadValue.load(),
  };

  if (result.rangeTotal != 12 || result.designatedValue != 21 ||
      result.threadValue != 34) {
    std::fprintf(stderr,
                 "RNQNX_G0_CXX20_FAIL ranges=%d designated=%d thread=%d\n",
                 result.rangeTotal,
                 result.designatedValue,
                 result.threadValue);
    return 1;
  }

  std::printf("RNQNX_G0_CXX20_OK ranges=12 designated=21 %s\n", threadBackend);
  return 0;
}
