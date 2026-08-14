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
  std::jthread worker([&threadValue] { threadValue.store(34); });
  worker.join();

  const ProbeResult result{
      .rangeTotal = rangeTotal,
      .designatedValue = doubled(10) + 1,
      .threadValue = threadValue.load(),
  };

  if (result.rangeTotal != 12 || result.designatedValue != 21 ||
      result.threadValue != 34) {
    std::fprintf(stderr,
                 "RNQNX_G0_CXX20_FAIL ranges=%d designated=%d jthread=%d\n",
                 result.rangeTotal,
                 result.designatedValue,
                 result.threadValue);
    return 1;
  }

  std::puts("RNQNX_G0_CXX20_OK ranges=12 designated=21 jthread=34");
  return 0;
}
