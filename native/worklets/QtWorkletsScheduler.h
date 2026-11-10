#pragma once
#include <worklets/Tools/UIScheduler.h>
#include <QObject>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace rnqnx::runtime {
// Actual upstream UIScheduler; this does not replace the Worklets runtime.
class QtWorkletsScheduler final : public worklets::UIScheduler,
    public std::enable_shared_from_this<QtWorkletsScheduler> {
 public:
  explicit QtWorkletsScheduler(std::function<void(std::string)> reportError);
  ~QtWorkletsScheduler() override;
  void scheduleOnUI(std::function<void()> job) override;
  void close();
 protected:
  bool queryIsOnUIThread() const override;
 private:
  void drain();
  // Serialize foreign-thread admission against owner-thread QObject teardown.
  std::mutex admission_;
  std::unique_ptr<QObject> context_;
  std::function<void(std::string)> reportError_;
  bool closed_{false};
};
}
