#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <map>
#include <signal.h>
#include <string>
#include <sys/time.h>
#include <ucontext.h>

namespace rnqnx_temporary_profile {
inline std::array<std::atomic<std::uintptr_t>, 60000> pcs{};
inline std::atomic<std::size_t> count{0};
static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
static_assert(std::atomic<std::size_t>::is_always_lock_free);

inline void sample(int, siginfo_t*, void* context) {
  const auto slot = count.fetch_add(1, std::memory_order_relaxed);
  if (slot < pcs.size()) {
    const auto* state = static_cast<const ucontext_t*>(context);
    pcs[slot].store(static_cast<std::uintptr_t>(state->uc_mcontext.gregs[REG_RIP]),
                    std::memory_order_relaxed);
  }
}

class Sampler {
 public:
  Sampler() {
    struct sigaction action{};
    action.sa_sigaction = sample;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGPROF, &action, &previousAction_) != 0) return;
    struct itimerval timer{};
    timer.it_interval.tv_usec = 10000;
    timer.it_value = timer.it_interval;
    active_ = setitimer(ITIMER_PROF, &timer, &previousTimer_) == 0;
    std::cerr << "RNQNX_TEMP_PC_SAMPLER active=" << active_ << " period_us=10000\n";
  }
  ~Sampler() {
    if (!active_) return;
    struct itimerval stopped{};
    setitimer(ITIMER_PROF, &stopped, nullptr);
    std::map<std::pair<std::string, std::uintptr_t>, std::size_t> histogram;
    const auto limit = std::min(count.load(std::memory_order_relaxed), pcs.size());
    for (std::size_t i = 0; i < limit; ++i) {
      const auto pc = pcs[i].load(std::memory_order_relaxed);
      Dl_info info{};
      if (pc && dladdr(reinterpret_cast<void*>(pc), &info) && info.dli_fname) {
        ++histogram[{info.dli_fname, pc - reinterpret_cast<std::uintptr_t>(info.dli_fbase)}];
      }
    }
    std::ofstream out("pc-samples.tsv");
    for (const auto& [key, samples] : histogram)
      out << key.first << '\t' << std::hex << key.second << '\t' << std::dec << samples << '\n';
    std::cerr << "RNQNX_TEMP_PC_SAMPLER samples=" << limit << " offsets=" << histogram.size() << "\n";
    sigaction(SIGPROF, &previousAction_, nullptr);
    setitimer(ITIMER_PROF, &previousTimer_, nullptr);
  }
 private:
  bool active_{false};
  struct sigaction previousAction_{};
  struct itimerval previousTimer_{};
};
} // namespace rnqnx_temporary_profile
