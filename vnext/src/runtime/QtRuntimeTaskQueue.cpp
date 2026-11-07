#include "react-native-qnx/runtime/QtRuntimeTaskQueue.h"

#include <QMetaObject>

#include <stdexcept>
#include <utility>

namespace rnqnx::runtime {

QtRuntimeTaskQueue::QtRuntimeTaskQueue(
    Execute execute, Task enterJS, Task leaveJS)
    : execute_(std::move(execute)),
      enterJS_(std::move(enterJS)),
      leaveJS_(std::move(leaveJS)) {}

QtRuntimeTaskQueue::~QtRuntimeTaskQueue() {
  // The host must close while its runtime is alive, on the owning thread.
  // Once closed, the last native reference may be released on any thread.
  if (context_) {
    if (!isOnOwnerThread()) {
      std::terminate();
    }
    close();
  }
}

bool QtRuntimeTaskQueue::isOnOwnerThread() const noexcept {
  return std::this_thread::get_id() == owner_;
}

void QtRuntimeTaskQueue::requireOwner() const {
  if (!isOnOwnerThread()) {
    throw std::runtime_error("JavaScript entry requires the owning Qt thread");
  }
}

void QtRuntimeTaskQueue::post(Task task) {
  const auto weak = weak_from_this();
  if (weak.expired()) {
    throw std::logic_error("Qt JavaScript task queue requires shared ownership");
  }
  std::unique_lock lock(mutex_);
  // A drained completion can resolve a native Promise which then posts its JS
  // settlement. Admit that owner-thread chain, but close worker admission.
  if (!accepting_ && !(isOnOwnerThread() && draining_)) {
    throw std::runtime_error("Qt JavaScript task queue is closed");
  }
  pending_.push_back(std::move(task));
  if (!wakePending_) {
    // Admission protects the receiver against concurrent owner-thread close.
    // QueuedConnection never executes user work while this mutex is held.
    if (!QMetaObject::invokeMethod(
            context_.get(),
            [weak]() {
              if (const auto queue = weak.lock()) {
                queue->drain();
              }
            },
            Qt::QueuedConnection)) {
      auto rejected = std::move(pending_.back());
      pending_.pop_back();
      lock.unlock();
      throw std::runtime_error("Unable to post to the Qt JavaScript event loop");
    }
    wakePending_ = true;
  }
}

void QtRuntimeTaskQueue::drain() {
  requireOwner();
  if (draining_) {
    return;
  }
  draining_ = true;
  struct ResetDraining {
    bool& flag;
    ~ResetDraining() { flag = false; }
  } reset{draining_};
  while (true) {
    Task task;
    {
      std::lock_guard lock(mutex_);
      if (pending_.empty()) {
        wakePending_ = false;
        break;
      }
      task = std::move(pending_.front());
      pending_.pop_front();
    }
    execute_(std::move(task));
  }
}

void QtRuntimeTaskQueue::runSync(Task task) {
  requireOwner();
  if (!context_) {
    throw std::runtime_error("Qt JavaScript task queue is closed");
  }
  enterJS();
  try {
    task();
  } catch (...) {
    leaveJS();
    throw;
  }
  leaveJS();
}

void QtRuntimeTaskQueue::enterJS() {
  requireOwner();
  if (!context_) {
    throw std::runtime_error("Qt JavaScript task queue is closed");
  }
  enterJS_();
  ++entryDepth_;
}

void QtRuntimeTaskQueue::leaveJS() noexcept {
  --entryDepth_;
  leaveJS_();
}

void QtRuntimeTaskQueue::close() {
  requireOwner();
  if (draining_ || entryDepth_ != 0) {
    throw std::runtime_error("Cannot close an active JavaScript task queue");
  }
  {
    std::lock_guard lock(mutex_);
    accepting_ = false;
  }
  if (!context_) {
    return;
  }
  // Do not cancel opaque Nitro awaitable closures: dropping them strands the
  // returned native Promise. Drain while Hermes and the RN scheduler are alive.
  drain();
  context_.reset();
  execute_ = {};
  enterJS_ = {};
  leaveJS_ = {};
}

}  // namespace rnqnx::runtime
