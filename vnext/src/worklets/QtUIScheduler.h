#pragma once

#include "react-native-qnx/worklets/QtUiThread.h"
#include "react-native-qnx/worklets/QtWorkletsPlatform.h"

#include <worklets/Tools/UIScheduler.h>

#include <memory>

namespace rnqnx::worklets {

/// Upstream's UIScheduler on the UI-worklet thread. Jobs queue in the base
/// class; one wake-up per batch is posted to the thread.
class QtUIScheduler final : public ::worklets::UIScheduler,
                            public std::enable_shared_from_this<QtUIScheduler> {
 public:
  QtUIScheduler(std::shared_ptr<QtUiThread> thread, std::shared_ptr<QtWorkletsStats> stats)
      : thread_(std::move(thread)), stats_(std::move(stats)) {}

  void scheduleOnUI(std::function<void()> job) override {
    if (isOnUIThread()) {
      stats_->uiJobsRun.fetch_add(1, std::memory_order_relaxed);
      job();
      return;
    }
    ::worklets::UIScheduler::scheduleOnUI(std::move(job));
    if (!scheduledOnUI_.exchange(true)) {
      thread_->post([weak = weak_from_this()]() {
        if (auto self = weak.lock()) {
          self->triggerUI();
        }
      });
    }
  }

  void triggerUI() override {
    ::worklets::UIScheduler::triggerUI();
  }

 protected:
  bool queryIsOnUIThread() const override { return thread_->isCurrent(); }

 private:
  std::shared_ptr<QtUiThread> thread_;
  std::shared_ptr<QtWorkletsStats> stats_;
};

}  // namespace rnqnx::worklets
