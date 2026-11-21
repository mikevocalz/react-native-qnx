#pragma once

#include <QObject>

#include <functional>
#include <memory>
#include <thread>

class QThread;

namespace rnqnx::runtime {

/// A thread with a Qt event loop that owns one JavaScript runtime.
///
/// ADR-0021. The thread exists only while a `QtReactHost` runs in JS-thread
/// mode. It is the single owner of Hermes, the RuntimeScheduler, JS timers and
/// the JavaScript task queue. Nothing else may touch them.
///
/// `post` never waits. `call` waits and is for the calls that need a result
/// before the caller can go on (starting a surface, evaluating a bundle). A GUI
/// handler that reacts to user input must not use `call`, since it would block
/// the GUI thread for as long as JavaScript stays busy.
class QtJsThread final {
 public:
  /// Starts the thread and returns once its event loop is running.
  QtJsThread();
  /// Quits the loop and joins. Work still queued is dropped, so the owner
  /// drains and tears down its runtime with `call` first.
  ~QtJsThread();

  QtJsThread(const QtJsThread&) = delete;
  QtJsThread& operator=(const QtJsThread&) = delete;

  bool isCurrent() const noexcept;

  /// Queues `work` and returns. Throws `std::runtime_error` when the thread is
  /// no longer running.
  void post(std::function<void()> work);

  /// Runs `work` on the JS thread and waits. Runs inline when already on it.
  /// An exception thrown by `work` is rethrown here. Must not be called from
  /// a thread the JS thread is itself waiting on.
  void call(const std::function<void()>& work);

  /// The receiver for queued calls. Objects that must live on the JS thread
  /// (timers, the task queue's wakeup target) are created inside `call`.
  QObject* context() const noexcept;

 private:
  class Worker;
  std::unique_ptr<QThread> thread_;
  std::unique_ptr<Worker> worker_;
  std::thread::id id_;
};

}  // namespace rnqnx::runtime
