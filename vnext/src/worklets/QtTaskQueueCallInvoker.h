#pragma once

#include "react-native-qnx/runtime/QtRuntimeTaskQueue.h"
#include "react-native-qnx/worklets/QtWorkletsPlatform.h"

#include <ReactCommon/CallInvoker.h>

#include <memory>
#include <stdexcept>

namespace rnqnx::worklets {

/// Upstream `JSScheduler` needs a CallInvoker that is safe from a non-JS thread.
/// This host's RuntimeScheduler executor runs inline on the caller, so its
/// invoker would execute JS on the UI-worklet thread. This one posts to the
/// host's JS owner queue instead, which runs the job on the owner thread inside
/// the scheduler's usual entry.
class QtTaskQueueCallInvoker final : public facebook::react::CallInvoker {
 public:
  QtTaskQueueCallInvoker(std::shared_ptr<runtime::QtRuntimeTaskQueue> queue,
                         std::shared_ptr<QtWorkletsStats> stats,
                         facebook::jsi::Runtime& runtime)
      : queue_(std::move(queue)), stats_(std::move(stats)), runtime_(runtime) {}

  void invokeAsync(facebook::react::CallFunc&& func) noexcept override {
    stats_->jsPosts.fetch_add(1, std::memory_order_relaxed);
    try {
      // The queue closes at host teardown; late posts are dropped, which is the
      // contract for work targeting a dead runtime.
      queue_->post([func = std::move(func), &rt = runtime_]() mutable { func(rt); });
    } catch (...) {
    }
  }

  void invokeSync(facebook::react::CallFunc&&) override {
    throw std::logic_error("invokeSync is not supported from the UI worklet thread");
  }

 private:
  std::shared_ptr<runtime::QtRuntimeTaskQueue> queue_;
  std::shared_ptr<QtWorkletsStats> stats_;
  facebook::jsi::Runtime& runtime_;
};

}  // namespace rnqnx::worklets
