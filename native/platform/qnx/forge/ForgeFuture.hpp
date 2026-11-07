#pragma once

#include "ForgeWorker.hpp"

#include <exception>
#include <future>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace rnqnx::forge::detail {

template <typename Result>
std::future<Result> rejectedFuture(std::exception_ptr error) {
  std::promise<Result> promise;
  promise.set_exception(std::move(error));
  return promise.get_future();
}

template <typename Function>
auto dispatch(const std::shared_ptr<ForgeWorker>& worker,
              const std::shared_ptr<nitroforge::native::ForgeSerialQueue>& queue,
              Function function) -> std::future<std::invoke_result_t<Function>> {
  using Result = std::invoke_result_t<Function>;
  try {
    if (!worker) {
      throw std::runtime_error("QNX Forge runtime is shut down");
    }
    auto task = std::make_shared<std::packaged_task<Result()>>(std::move(function));
    auto result = task->get_future();
    worker->submit(queue, [task] { (*task)(); });
    return result;
  } catch (...) {
    return rejectedFuture<Result>(std::current_exception());
  }
}

template <typename Result>
std::future<Result> closedDocument() {
  return rejectedFuture<Result>(
      std::make_exception_ptr(std::runtime_error("QNX Forge document is closed")));
}

} // namespace rnqnx::forge::detail
