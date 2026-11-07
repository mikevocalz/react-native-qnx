#include "QnxForgeDocument.hpp"
#include "QnxForgeRuntime.hpp"

#include <algorithm>
#include <barrier>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using rnqnx::forge::QnxForgeDocument;
using rnqnx::forge::QnxForgeRuntime;
using nitroforge::native::CommandResult;

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Future>
decltype(auto) complete(Future&& future) {
  require(future.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
          "native future did not settle before timeout");
  return future.get();
}

template <typename Future>
void rejects(Future&& future, std::string_view expected = {}) {
  require(future.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
          "failed native future did not settle before timeout");
  try {
    future.get();
  } catch (const std::exception& error) {
    require(std::string_view(error.what()).find(expected) != std::string_view::npos,
            "failure did not include the expected diagnostic");
    return;
  }
  throw std::runtime_error("operation should have rejected");
}

void value(const std::shared_ptr<QnxForgeDocument>& document, std::string_view expected) {
  const auto result = complete(document->apply("evaluate", R"({"formula":"=B1"})"));
  require(result.dataJson == expected, "unexpected Tally formula value: " + result.dataJson);
}

void editUndoAndOwnership() {
  QnxForgeRuntime runtime;
  rejects(runtime.open("not-an-engine"));
  auto document = complete(runtime.open("tally"));
  auto independent = complete(runtime.open("tally"));

  std::string payload = R"({"addr":"A1","value":"10"})";
  auto first = document->apply("setCell", payload);
  payload.assign("destroy caller-owned storage");
  auto second = document->apply("setCell", R"({"addr":"A2","value":"20"})");
  auto formula = document->apply("setCell", R"json({"addr":"B1","value":"=SUM(A1:A2)"})json");
  value(document, R"({"value":30.0})");
  const auto changed = complete(first);
  require(std::any_of(changed.events.begin(), changed.events.end(), [](const auto& event) {
    return event.kind == "document:changed";
  }), "real edit must return its owned Rust mutation event");
  complete(second);
  complete(formula);

  auto edit = document->apply("setCell", R"({"addr":"A1","value":"22"})");
  auto undo = document->apply("__undo");
  value(document, R"({"value":30.0})");
  complete(edit);
  require(complete(undo).dataJson == R"({"undone":true})", "undo must follow its edit");
  complete(document->apply("__redo"));
  value(document, R"({"value":42.0})");

  complete(independent->apply("setCell", R"({"addr":"B1","value":"99"})"));
  value(independent, R"({"value":99.0})");
  value(document, R"({"value":42.0})");

  const auto before = complete(document->snapshot());
  auto invalid = document->apply("setCell", R"({"addr":"A0","value":"999"})");
  auto afterInvalid = document->apply("evaluate", R"({"formula":"=B1"})");
  rejects(invalid);
  require(complete(afterInvalid).dataJson == R"({"value":42.0})",
          "an engine exception must not strand the following operation");
  rejects(document->apply("setCell", "{broken-json"));
  rejects(document->apply("setCell", R"({"addr":"A1","value":"999"})", 999));
  auto corrupt = before;
  corrupt.front() = 'X';
  rejects(document->restore(corrupt));
  require(complete(document->snapshot()) == before, "failed inputs must preserve the document");

  auto restored = complete(runtime.open("tally"));
  auto bytes = before;
  auto restore = restored->restore(bytes);
  bytes.assign(10, 0);
  complete(restore);
  value(restored, R"({"value":42.0})");
  require(complete(restored->apply("__undo")).dataJson == R"({"undone":false})",
          "restore must begin a new undo journal");

  auto pending = document->apply("setCell", R"({"addr":"A1","value":"23"})");
  auto closed = document->close();
  auto closedAgain = document->close();
  rejects(document->snapshot(), "document is closed");
  rejects(document->apply("__undo"), "document is closed");
  complete(pending);
  complete(closed);
  complete(closedAgain);
  value(independent, R"({"value":99.0})");
  // Rust result buffers are copied: they still exist after the source closes.
  require(changed.events.front().kind == "document:changed", "result storage must outlive close");
  runtime.shutdown();
  runtime.shutdown();
  rejects(independent->snapshot(), "runtime is shut down");
  rejects(runtime.open("tally"), "runtime is shut down");
}

void concurrentSubmissionAndShutdown() {
  QnxForgeRuntime runtime;
  auto document = complete(runtime.open("tally"));
  constexpr int threadCount = 4;
  constexpr int editsPerThread = 80;
  std::barrier start(threadCount + 1);
  std::vector<std::vector<std::future<CommandResult>>> results(threadCount);
  std::vector<std::thread> threads;
  for (int index = 0; index < threadCount; ++index) {
    threads.emplace_back([&, index] {
      start.arrive_and_wait();
      for (int edit = 1; edit <= editsPerThread; ++edit) {
        const auto payload = "{\"addr\":\"A" + std::to_string(index + 1) +
            "\",\"value\":\"" + std::to_string(edit) + "\"}";
        results[index].push_back(document->apply("setCell", payload));
      }
    });
  }
  start.arrive_and_wait();
  for (auto& thread : threads) {
    thread.join();
  }
  auto finalValue = document->apply("evaluate", R"({"formula":"=A1+A2+A3+A4"})");
  runtime.shutdown();
  for (auto& group : results) {
    for (auto& result : group) {
      complete(result);
    }
  }
  require(complete(finalValue).dataJson == R"({"value":320.0})",
          "shutdown must drain every accepted edit in each caller's submission order");
  rejects(document->snapshot(), "runtime is shut down");
}

void droppedHandlesAndTeardownRace() {
  std::shared_ptr<QnxForgeDocument> survivor;
  std::future<std::vector<uint8_t>> pendingSnapshot;
  {
    QnxForgeRuntime runtime;
    auto document = complete(runtime.open("tally"));
    // Discarding a future or a document does not cancel accepted commands.
    document->apply("setCell", R"({"addr":"B1","value":"42"})");
    pendingSnapshot = document->snapshot();
    document.reset();
    survivor = complete(runtime.open("tally"));
  }
  const auto bytes = complete(pendingSnapshot);
  rejects(survivor->snapshot(), "runtime is shut down");

  QnxForgeRuntime reopenedRuntime;
  auto reopened = complete(reopenedRuntime.open("tally"));
  complete(reopened->restore(bytes));
  value(reopened, R"({"value":42.0})");

  std::barrier start(2);
  std::vector<std::future<CommandResult>> results;
  std::thread caller([&] {
    start.arrive_and_wait();
    for (int edit = 0; edit < 100; ++edit) {
      results.push_back(reopened->apply("setCell", R"({"addr":"A1","value":"9"})"));
    }
  });
  start.arrive_and_wait();
  reopenedRuntime.shutdown();
  caller.join();
  for (auto& result : results) {
    require(result.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "shutdown race stranded a future");
    try {
      result.get();
    } catch (const std::exception& error) {
      require(std::string_view(error.what()).find("runtime is shut down") != std::string_view::npos,
              "shutdown race may only reject work that missed admission");
    }
  }
}

} // namespace

int main() {
  try {
    editUndoAndOwnership();
    concurrentSubmissionAndShutdown();
    droppedHandlesAndTeardownRace();
    std::cout << "PASS host QNX Forge seam: real Tally edit/undo, copied inputs/results, isolation, "
                 "restore, concurrent FIFO, close, drain and teardown rejection\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL host QNX Forge seam: " << error.what() << '\n';
    return 1;
  }
}
