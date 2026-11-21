#include "react-native-qnx/runtime/QtJsThread.h"

#include "react-native-qnx/runtime/QueuedCallTarget.h"

#include <QDebug>
#include <QThread>

#include <condition_variable>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace rnqnx::runtime {

namespace {
// Hermes recurses on the native stack. Secondary threads default to 512 KiB on
// macOS and QNX, far below the main thread's 8 MiB, so a deep but legal script
// would overflow here and not on the GUI thread. Asking for the main thread's
// size keeps JS-thread mode from changing what a script may do.
constexpr unsigned kJsThreadStackBytes = 8U * 1024U * 1024U;
}  // namespace

/// Lives on the JS thread; gives queued calls a receiver with that affinity.
class QtJsThread::Worker final : public QueuedCallTarget {};

QtJsThread::QtJsThread() : thread_(std::make_unique<QThread>()) {
  thread_->setObjectName(QStringLiteral("rnqnx-js"));
  thread_->setStackSize(kJsThreadStackBytes);
  worker_ = std::make_unique<Worker>();
  worker_->moveToThread(thread_.get());

  std::mutex mutex;
  std::condition_variable ready;
  bool running = false;
  std::thread::id startedOn;
  // Runs first on the new thread, inside exec(), so `running` means the event
  // loop accepts queued calls.
  worker_->postCall([&] {
    std::lock_guard lock(mutex);
    startedOn = std::this_thread::get_id();
    running = true;
    ready.notify_one();
  });
  thread_->start();
  std::unique_lock lock(mutex);
  ready.wait(lock, [&] { return running; });
  id_ = startedOn;
}

QtJsThread::~QtJsThread() {
  if (isCurrent()) {
    // Destroying the thread from itself would join itself.
    std::terminate();
  }
  thread_->quit();
  thread_->wait();
  // The worker has thread affinity to a stopped thread. Deleting it from here
  // is allowed once the thread has finished, and drops any posted events.
  worker_.reset();
}

bool QtJsThread::isCurrent() const noexcept {
  return std::this_thread::get_id() == id_;
}

QObject* QtJsThread::context() const noexcept {
  return worker_.get();
}

void QtJsThread::post(std::function<void()> work) {
  if (!thread_->isRunning()) {
    throw std::runtime_error("The JavaScript thread is not running");
  }
  worker_->postCall([work = std::move(work)]() {
    // An exception leaving a Qt event-loop functor ends the process. Posted
    // work has no caller to receive it, so report it and carry on.
    try {
      work();
    } catch (const std::exception& error) {
      qWarning() << "Uncaught exception in work posted to the JS thread:" << error.what();
    } catch (...) {
      qWarning() << "Uncaught non-standard exception in work posted to the JS thread";
    }
  });
}

void QtJsThread::call(const std::function<void()>& work) {
  if (isCurrent()) {
    work();
    return;
  }
  if (!thread_->isRunning()) {
    throw std::runtime_error("The JavaScript thread is not running");
  }
  // The caller's stack stays alive until `finished`, so capturing by reference
  // is sound. The mutex and condition variable carry the ordering edge back to
  // the caller; no Qt blocking connection is involved.
  std::exception_ptr failure;
  std::mutex mutex;
  std::condition_variable finishedSignal;
  bool finished = false;
  worker_->postCall([&] {
    try {
      work();
    } catch (...) {
      failure = std::current_exception();
    }
    std::lock_guard lock(mutex);
    finished = true;
    finishedSignal.notify_one();
  });
  {
    std::unique_lock lock(mutex);
    finishedSignal.wait(lock, [&] { return finished; });
  }
  if (failure) {
    std::rethrow_exception(failure);
  }
}

}  // namespace rnqnx::runtime
