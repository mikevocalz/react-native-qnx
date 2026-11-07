#include "react-native-qnx/runtime/RuntimeTrace.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>

using namespace rnqnx::runtime;

namespace {
void require(bool valid, const char* failure) {
  if (!valid) throw std::runtime_error(failure);
}
void boundedAndFrozen() {
  RuntimeTrace trace(2);
  trace.record(TraceStage::QueueAdmission, TracePhase::Instant, 41);
  trace.record(TraceStage::QueueAdmission, TracePhase::Instant, 42);
  trace.record(TraceStage::QueueAdmission, TracePhase::Instant, 43);
  const auto first = trace.freeze();
  require(first.events.size() == 2 && first.dropped == 1, "capacity/drop accounting");
  require(first.events[0].correlation == 41 && first.events[1].correlation == 42, "first records retained");
  trace.record(TraceStage::Evaluate, TracePhase::Begin, 99);
  const auto second = trace.freeze();
  require(second.events.size() == 2 && second.dropped == 1, "freeze prevents future records");
  for (const auto capacity : {0U, 65537U}) {
    bool rejected = false;
    try { RuntimeTrace invalid(capacity); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "invalid capacity rejected");
  }
}
void runWorkers(RuntimeTrace& trace, unsigned count, unsigned records) {
  std::mutex mutex;
  std::condition_variable changed;
  unsigned ready = 0;
  bool released = false;
  std::vector<std::thread> workers;
  workers.reserve(count);
  const auto releaseAndJoin = [&] {
    { std::lock_guard lock(mutex); released = true; }
    changed.notify_all();
    for (auto& worker : workers) if (worker.joinable()) worker.join();
  };
  try {
    for (unsigned i = 0; i < count; ++i) workers.emplace_back([&] {
      {
        std::unique_lock lock(mutex);
        ++ready;
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
      }
      for (unsigned i = 0; i < records; ++i) {
        trace.record(TraceStage::QueueAdmission, TracePhase::Instant, trace.nextId());
      }
    });
    bool started;
    {
      std::unique_lock lock(mutex);
      started = changed.wait_for(lock, std::chrono::seconds(3), [&] { return ready == count; });
    }
    releaseAndJoin();
    require(started, "worker startup predicate");
  } catch (...) {
    releaseAndJoin();
    throw;
  }
}
void concurrentOrder() {
  RuntimeTrace trace(1000);
  runWorkers(trace, 4, 100);
  auto snapshot = trace.freeze();
  require(snapshot.events.size() == 400 && snapshot.dropped == 0, "concurrent accounting");
  std::set<unsigned> threads;
  std::set<std::uint64_t> correlations;
  std::uint64_t timestamp = 0;
  for (std::size_t i = 0; i < snapshot.events.size(); ++i) {
    const auto& event = snapshot.events[i];
    require(event.sequence == i + 1 && event.recordedAtNs >= timestamp, "monotonic record order");
    timestamp = event.recordedAtNs;
    threads.insert(event.thread);
    correlations.insert(event.correlation);
  }
  require(threads.size() == 4 && correlations.size() == 400, "thread/correlation identities");
  RuntimeTrace overflow(17);
  runWorkers(overflow, 4, 100);
  const auto full = overflow.freeze();
  require(full.events.size() == 17 && full.dropped == 383, "concurrent overflow accounting");
  RuntimeTrace threadLimit(100);
  runWorkers(threadLimit, 65, 1);
  const auto limited = threadLimit.freeze();
  require(limited.events.size() == 64 && limited.dropped == 1, "bounded thread registration");
}
class GroupedNumbers final : public std::numpunct<char> {
 protected:
  char do_thousands_sep() const override { return ','; }
  std::string do_grouping() const override { return "\3"; }
};
void spansAndExport() {
  auto trace = std::make_shared<RuntimeTrace>(20);
  {
    TraceSpan span(trace, TraceStage::SurfaceStart, 11, 101);
  }
  try {
    TraceSpan span(trace, TraceStage::Evaluate);
    throw std::runtime_error("private test payload must not be exported");
  } catch (const std::runtime_error&) {}
  const auto snapshot = trace->freeze();
  require(snapshot.events.size() == 4, "paired spans");
  require(snapshot.events[0].correlation == snapshot.events[1].correlation &&
          snapshot.events[0].surfaceGeneration == 101, "span/generation identity");
  require(!snapshot.events[1].unwinding && snapshot.events[3].unwinding, "unwind boundary flag");
  std::ostringstream output;
  RuntimeTrace::writeJson(snapshot, output);
  require(output.str().find("private test payload") == std::string::npos, "no exception content");
  require(output.str().find("\"schemaVersion\":1") != std::string::npos &&
          output.str().find("session-relative") != std::string::npos, "versioned clock contract");
  std::ostringstream broken;
  broken.setstate(std::ios::badbit);
  bool rejected = false;
  try { RuntimeTrace::writeJson(snapshot, broken); } catch (const std::runtime_error&) { rejected = true; }
  require(rejected, "export error surfaced");
  auto localized = snapshot;
  localized.session = 1234567;
  std::ostringstream grouped;
  grouped.imbue(std::locale(std::locale::classic(), new GroupedNumbers));
  RuntimeTrace::writeJson(localized, grouped);
  require(grouped.str().find("\"session\":1234567") != std::string::npos &&
          grouped.str().find("1,234,567") == std::string::npos, "locale-independent JSON numbers");
  RuntimeTrace another(1);
  require(another.freeze().session != snapshot.session, "session identity");
}
} // namespace
int main(int argc, char** argv) {
  try {
    boundedAndFrozen();
    concurrentOrder();
    spansAndExport();
    if (argc == 2 && std::string_view(argv[1]) == "--json") {
      RuntimeTrace trace(2);
      trace.record(TraceStage::QueueAdmission, TracePhase::Instant, 17);
      RuntimeTrace::writeJson(trace.freeze(), std::cout);
    } else {
      std::cout << "RNQNX_RUNTIME_TRACE_OK capacity drop order thread span freeze export\n";
    }
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_RUNTIME_TRACE_FAIL " << error.what() << '\n';
    return 1;
  }
}
