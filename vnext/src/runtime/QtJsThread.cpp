#include "react-native-qnx/runtime/QtJsThread.h"

#include <QMetaObject>
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
class QtJsThread::Worker final : public QObject {};

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
  QMetaObject::invokeMethod(
      worker_.get(),
      [&] {
        std::lock_guard lock(mutex);
        startedOn = std::this_thread::get_id();
        running = true;
        ready.notify_one();
      },
      Qt::QueuedConnection);
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
  QMetaObject::invokeMethod(
      worker_.get(), [work = std::move(work)]() { work(); }, Qt::QueuedConnection);
}

void QtJsThread::call(const std::function<void()>& work) {
  if (isCurrent()) {
    work();
    return;
  }
  if (!thread_->isRunning()) {
    throw std::runtime_error("The JavaScript thread is not running");
  }
  std::exception_ptr failure;
  // BlockingQueuedConnection keeps the caller's stack alive for the whole run,
  // so capturing by reference is sound.
  QMetaObject::invokeMethod(
      worker_.get(),
      [&] {
        try {
          work();
        } catch (...) {
          failure = std::current_exception();
        }
      },
      Qt::BlockingQueuedConnection);
  if (failure) {
    std::rethrow_exception(failure);
  }
}

}  // namespace rnqnx::runtime
