#include "react-native-qnx/worklets/QtUiThread.h"

#include <QMetaObject>
#include <QObject>
#include <QThread>

#include <stdexcept>
#include <utility>

namespace rnqnx::worklets {

QtUiThread::QtUiThread(const QString& name)
    : thread_(std::make_unique<QThread>()), context_(std::make_unique<QObject>()) {
  thread_->setObjectName(name);
  context_->moveToThread(thread_.get());
  // Record the id from inside the thread so isCurrent() never races start().
  QObject::connect(thread_.get(), &QThread::started, context_.get(),
                   [this]() { id_.store(std::this_thread::get_id()); },
                   Qt::DirectConnection);
  thread_->start();
  // Callers compare against id(); wait for the thread to publish it.
  while (id_.load() == std::thread::id{}) {
    std::this_thread::yield();
  }
}

QtUiThread::~QtUiThread() {
  stop();
}

bool QtUiThread::post(std::function<void()> job) {
  std::lock_guard lock(admissionMutex_);
  if (!accepting_.load(std::memory_order_acquire)) {
    return false;
  }
  return QMetaObject::invokeMethod(
      context_.get(), [this, job = std::move(job)]() {
        if (accepting_.load(std::memory_order_acquire)) job();
      }, Qt::QueuedConnection);
}

bool QtUiThread::isCurrent() const noexcept {
  return std::this_thread::get_id() == id_.load();
}

void QtUiThread::stop() {
  if (isCurrent()) {
    throw std::logic_error("QtUiThread::stop called from the UI thread");
  }
  std::call_once(stopped_, [this]() {
    {
      // A poster that obtained the receiver finishes enqueueing before closure.
      // Later posters return without reading it, including during destruction.
      std::lock_guard lock(admissionMutex_);
      accepting_.store(false, std::memory_order_release);
    }
    // A running job may post again, so never join while holding admission.
    thread_->quit();
    thread_->wait();
    // Pending posted events die with the context object, never running.
    context_.reset();
  });
}

}  // namespace rnqnx::worklets
