#pragma once

#include "react-native-qnx/runtime/RuntimeTrace.h"

#include <QObject>

#include <list>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace rnqnx::runtime {

/// Posts native completions to the host's owning Qt/JavaScript thread.
/// The host closes admission and drains accepted work before destroying Hermes.
/// Retained queues reject later posts without retaining the runtime.
class QtRuntimeTaskQueue final
    : public std::enable_shared_from_this<QtRuntimeTaskQueue> {
 public:
  using Task = std::function<void()>;
  using Execute = std::function<void(Task)>;

  QtRuntimeTaskQueue(Execute execute, Task enterJS, Task leaveJS);
#if RNQNX_ENABLE_FRAME_TRACE
  QtRuntimeTaskQueue(Execute execute, Task enterJS, Task leaveJS,
                     std::shared_ptr<RuntimeTrace> trace);
#endif
  ~QtRuntimeTaskQueue();

  void post(Task task);
  void runSync(Task task);
  bool isOnOwnerThread() const noexcept;
  void enterJS();
  void leaveJS() noexcept;

  /// Owner-thread, idle-boundary teardown. Runs every accepted completion once.
  void close();

 private:
  void drain();
  void requireOwner() const;

  const std::thread::id owner_{std::this_thread::get_id()};
  std::mutex mutex_;
  bool accepting_{true};
  bool wakePending_{false};
  bool draining_{false};  // Owner-thread only; nested Qt pumping cannot reorder.
  unsigned entryDepth_{0};  // Owner-thread only.
  // Distinct from RN's uninstrumented deque<Task> template in host ASan builds.
  std::list<Task> pending_;
  std::unique_ptr<QObject> context_{std::make_unique<QObject>()};
#if RNQNX_ENABLE_FRAME_TRACE
  std::shared_ptr<RuntimeTrace> trace_;
#endif
  Execute execute_;
  Task enterJS_;
  Task leaveJS_;
};

}  // namespace rnqnx::runtime
