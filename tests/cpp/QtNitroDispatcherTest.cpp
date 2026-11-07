#include "QnxNitroRuntime.hpp"
#include "QtNitroDispatcher.hpp"
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <NitroModules/JSIConverter+Callback.hpp>
#include <NitroModules/JSIConverter+Promise.hpp>

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>

#include <atomic>
#include <barrier>
#include <chrono>
#include <future>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iostream>
#include <optional>
#if RNQNX_ENABLE_FRAME_TRACE
#include <map>
#endif
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace margelo::nitro;
using namespace std::chrono_literals;
using rnqnx::runtime::QtReactHost;

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void evaluate(QtReactHost& host, const char* fixture) {
  const char* configured = std::getenv("RNQNX_NITRO_QT_FIXTURES");
  const std::string directory = configured ? configured : RNQNX_NITRO_QT_FIXTURE_DIR;
#ifdef __QNX__
  const std::string path = directory + "/" + fixture + ".hbc";
#else
  const std::string path = directory + "/" + fixture + ".js";
#endif
  std::ifstream file(path, std::ios::binary);
  require(file.is_open(), "Cannot open Nitro Qt test fixture");
  const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  if (!host.evaluate(source, path)) {
    throw std::runtime_error(host.lastError().toStdString());
  }
}

template <typename Ready>
void pumpUntil(Ready ready) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!ready() && std::chrono::steady_clock::now() < deadline) {
    QCoreApplication::processEvents();
  }
  require(ready(), "Qt completion timed out");
}

double number(QtReactHost& host, const char* name) {
  return host.runtime().global().getProperty(host.runtime(), name).asNumber();
}

void exercise(rnqnx::fabric::ShadowViewMutationAdapter& adapter) {
#if RNQNX_ENABLE_FRAME_TRACE
  auto trace = std::make_shared<rnqnx::runtime::RuntimeTrace>(1000);
  auto host = std::make_unique<QtReactHost>(adapter, trace);
#else
  auto host = std::make_unique<QtReactHost>(adapter);
#endif
  auto dispatcher = std::make_shared<rnqnx::nitro::QtNitroDispatcher>(host->taskQueue());
  rnqnx::nitro::installQnxNitroRuntime(host->runtime(), dispatcher);
  const auto owner = std::this_thread::get_id();
  host->runtime().global().setProperty(host->runtime(), "queueMicrotask",
      facebook::jsi::Function::createFromHostFunction(
          host->runtime(), facebook::jsi::PropNameID::forAscii(host->runtime(), "queueMicrotask"), 1,
          [](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
             const facebook::jsi::Value* args, size_t) {
            rt.queueMicrotask(args[0].asObject(rt).asFunction(rt));
            return facebook::jsi::Value::undefined();
          }));
  evaluate(*host, "setup");
  auto callback = JSIConverter<Callback<double()>>::fromJSI(
      host->runtime(), host->runtime().global().getProperty(host->runtime(), "callback"));
  std::optional<std::future<double>> result;
  std::atomic<bool> workerSyncRejected{false};
  std::thread worker([&]() {
    require(!dispatcher->isOnDispatcher(), "worker identified as JS owner");
    try {
      dispatcher->runSync([]() {});
    } catch (const std::runtime_error&) {
      workerSyncRejected = true;
    }
    result.emplace(callback.callAsync().await());
  });
  worker.join();
  require(workerSyncRejected, "off-thread synchronous JS was accepted");
  require(number(*host, "calls") == 0, "async callback executed inline on worker");
  pumpUntil([&]() { return result->wait_for(0s) == std::future_status::ready; });
  require(result->get() == 42 && number(*host, "microtasks") == 1,
          "worker callback or RN microtask checkpoint failed");
  require(callback.callSync() == 42 && number(*host, "microtasks") == 2,
          "direct native-to-JS entry did not checkpoint");

  auto failure = JSIConverter<Callback<double()>>::fromJSI(
      host->runtime(), host->runtime().global().getProperty(host->runtime(), "fail"));
  try {
    (void)failure.callSync();
    throw std::logic_error("throwing callback succeeded");
  } catch (const facebook::jsi::JSError&) {
  }
  auto microtaskFailure = JSIConverter<Callback<double()>>::fromJSI(
      host->runtime(), host->runtime().global().getProperty(host->runtime(), "failMicrotask"));
  const auto errors = host->javaScriptErrors().size();
  require(microtaskFailure.callSync() == 42, "microtask callback result changed");
  require(host->javaScriptErrors().size() == errors + 1,
          "microtask error escaped or was lost");
  evaluate(*host, "recover");

  auto pending = Promise<double>::pending();
  host->runtime().global().setProperty(host->runtime(), "nativePending",
      JSIConverter<Promise<double>>::toJSI(host->runtime(), pending));
  evaluate(*host, "resolve");
  std::thread resolver([pending]() { pending.resolve(42); });
  resolver.join();
  pumpUntil([&]() { return number(*host, "resolved") == 42; });

  auto rejected = Promise<double>::pending();
  host->runtime().global().setProperty(host->runtime(), "nativeRejected",
      JSIConverter<Promise<double>>::toJSI(host->runtime(), rejected));
  evaluate(*host, "reject");
  std::thread rejecter([rejected]() {
    rejected.reject(std::make_exception_ptr(std::runtime_error("native failure")));
  });
  rejecter.join();
  pumpUntil([&]() { return number(*host, "rejected") == 42; });

  auto pendingVoid = Promise<void>::pending();
  host->runtime().global().setProperty(host->runtime(), "nativeVoid",
      JSIConverter<Promise<void>>::toJSI(host->runtime(), pendingVoid));
  evaluate(*host, "void");
  std::thread voidResolver([pendingVoid]() { pendingVoid.resolve(); });
  voidResolver.join();
  pumpUntil([&]() { return number(*host, "voidResolved") == 42; });

  std::vector<int> order;
  std::atomic<bool> captureReleasedOnOwner{false};
  std::thread captureProducer([&]() {
    auto capture = std::shared_ptr<int>(new int(1), [&](int* value) {
      delete value;
      captureReleasedOnOwner = std::this_thread::get_id() == owner;
    });
    dispatcher->runAsync([capture]() {});
  });
  captureProducer.join();
  dispatcher->runAsync([&]() {
    require(std::this_thread::get_id() == owner, "completion ran off owner");
    order.push_back(1);
    QCoreApplication::processEvents();
  });
  dispatcher->runAsync([&]() { order.push_back(2); });
  auto chained = dispatcher->runAsyncAwaitable([]() { return 42.0; });
  host->runtime().global().setProperty(host->runtime(), "chained",
      JSIConverter<Promise<double>>::toJSI(host->runtime(), chained));
  bool chainedSettled = false;
  host->runtime().global().setProperty(host->runtime(), "recordChained",
      facebook::jsi::Function::createFromHostFunction(
          host->runtime(), facebook::jsi::PropNameID::forAscii(host->runtime(), "recordChained"), 1,
          [&](facebook::jsi::Runtime&, const facebook::jsi::Value&,
              const facebook::jsi::Value* args, size_t) {
            chainedSettled = args[0].asNumber() == 42;
            return facebook::jsi::Value::undefined();
          }));
  evaluate(*host, "chained");
  bool stoppedBeforeQueueClose = false;
  host->startEmptySurface(1, 640, 480);
  host->runtime().global().setProperty(host->runtime(), "RN$stopSurface",
      facebook::jsi::Function::createFromHostFunction(
          host->runtime(), facebook::jsi::PropNameID::forAscii(host->runtime(), "stopSurface"), 1,
          [callback, &stoppedBeforeQueueClose](facebook::jsi::Runtime&,
              const facebook::jsi::Value&, const facebook::jsi::Value*, size_t) {
            stoppedBeforeQueueClose = callback.callSync() == 42;
            return facebook::jsi::Value::undefined();
          }));
  auto rejectedCallback = failure.callAsync().await();
  auto drained = callback.callAsync().await();
  host.reset();  // No event pumping: teardown must finish accepted awaitables.
  require(stoppedBeforeQueueClose, "surface cleanup ran after dispatcher close");
  require(drained.wait_for(0s) == std::future_status::ready && drained.get() == 42,
          "teardown stranded an accepted native Promise");
  require(order == std::vector<int>({1, 2}), "teardown reordered accepted work");
  require(chainedSettled, "teardown rejected transitive JS Promise settlement");
  require(captureReleasedOnOwner, "accepted task capture was released off owner");
  require(rejectedCallback.wait_for(0s) == std::future_status::ready,
          "throwing async callback stranded its native Promise");
  try {
    (void)rejectedCallback.get();
    throw std::logic_error("throwing async callback succeeded");
  } catch (const std::runtime_error& error) {
    require(std::string(error.what()).find("callback failure") != std::string::npos,
            "async callback lost its error after Hermes teardown");
  }
  auto late = callback.callAsync().await();
  require(late.wait_for(0s) == std::future_status::ready,
          "closed dispatcher left a native Promise pending");
  try {
    (void)late.get();
    throw std::logic_error("closed dispatcher accepted callback");
  } catch (const std::runtime_error&) {
  }
  callback.callAndForget();  // Closed admission must not throw into native code.
  QCoreApplication::processEvents();  // Posted wakeup must not touch dead Hermes.
#if RNQNX_ENABLE_FRAME_TRACE
  using rnqnx::runtime::TracePhase;
  using rnqnx::runtime::TraceStage;
  const auto snapshot = trace->freeze();
  require(snapshot.dropped == 0, "Nitro trace overflowed");
  struct Counts {
    unsigned admitted = 0;
    unsigned began = 0;
    unsigned ended = 0;
    unsigned producer = 0;
    unsigned executor = 0;
    unsigned endThread = 0;
    std::uint64_t admittedSequence = 0;
    std::uint64_t beginSequence = 0;
    std::uint64_t endSequence = 0;
  };
  std::map<std::uint64_t, Counts> operations;
  unsigned ownerTraceThread = 0;
  bool closeEnded = false;
  for (const auto& event : snapshot.events) {
    if (event.stage == TraceStage::Evaluate && ownerTraceThread == 0) ownerTraceThread = event.thread;
    if (event.stage == TraceStage::QueueAdmission) {
      auto& operation = operations[event.correlation];
      ++operation.admitted;
      operation.producer = event.thread;
      operation.admittedSequence = event.sequence;
      require(event.phase == TracePhase::Instant, "Nitro trace admission phase changed");
    } else if (event.stage == TraceStage::QueueExecute) {
      auto& operation = operations[event.correlation];
      if (event.phase == TracePhase::Begin) {
        ++operation.began;
        operation.executor = event.thread;
        operation.beginSequence = event.sequence;
      } else if (event.phase == TracePhase::End) {
        ++operation.ended;
        operation.endThread = event.thread;
        operation.endSequence = event.sequence;
      }
    } else if (event.stage == TraceStage::QueueClose && event.phase == TracePhase::End) {
      closeEnded = true;
    }
  }
  require(ownerTraceThread != 0 && closeEnded && !operations.empty(), "Nitro trace missed owner/close/admission");
  bool crossThread = false;
  for (const auto& [correlation, operation] : operations) {
    require(correlation != 0 && operation.admitted == 1 && operation.began == 1 && operation.ended == 1,
            "Nitro trace admission/execution correlation mismatch");
    require(operation.executor == ownerTraceThread && operation.endThread == ownerTraceThread,
            "Nitro trace execution was not on owner");
    require(operation.admittedSequence < operation.beginSequence && operation.beginSequence < operation.endSequence,
            "Nitro trace admission/execution order changed");
    crossThread = crossThread || operation.producer != operation.executor;
  }
  require(crossThread, "Nitro trace missed actual worker admission");
  std::cout << "RNQNX_NITRO_TRACE_OK admissions=" << operations.size()
            << " executed=" << operations.size() << " dropped=0 cross-thread=1 close=1\n";
#endif
}

void raceClose(rnqnx::fabric::ShadowViewMutationAdapter& adapter) {
  for (int attempt = 0; attempt < 30; ++attempt) {
    auto host = std::make_unique<QtReactHost>(adapter);
    auto dispatcher = std::make_shared<rnqnx::nitro::QtNitroDispatcher>(host->taskQueue());
    rnqnx::nitro::installQnxNitroRuntime(host->runtime(), dispatcher);
    std::barrier start(2);
    std::vector<std::future<int>> results;
    const auto owner = std::this_thread::get_id();
    std::thread producer([&]() {
      for (int i = 0; i < 100; ++i) {
        results.push_back(dispatcher->runAsyncAwaitable([owner]() {
          require(std::this_thread::get_id() == owner, "race task ran on worker");
          return 42;
        }).await());
        if (i == 0) {
          start.arrive_and_wait();  // Guarantee one accepted task before close.
        }
      }
    });
    start.arrive_and_wait();
    host.reset();
    producer.join();
    for (size_t index = 0; index < results.size(); ++index) {
      auto& result = results[index];
      require(result.wait_for(0s) == std::future_status::ready, "race stranded native Promise");
      try {
        require(result.get() == 42, "race result corrupted");
      } catch (const std::runtime_error& error) {
        require(index != 0, "pre-admitted race task did not drain");
        require(std::string(error.what()).find("closed") != std::string::npos,
                "race error was not closed admission");
      }
    }
    QCoreApplication::processEvents();
  }
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    QQmlEngine engine;
    QQuickItem root;
    rnqnx::views::QtQuickItemFactory factory(engine);
    rnqnx::fabric::QtQuickMountingLayer layer(factory, root, 1);
    rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
    exercise(adapter);
    raceClose(adapter);
    std::cout << "RNQNX_NITRO_QT_OK worker callback promise error checkpoint drain close-race\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL Qt Nitro dispatcher: " << error.what() << '\n';
    return 1;
  }
}
