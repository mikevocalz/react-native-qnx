#include "QtWorkletsScheduler.h"
#include <QCoreApplication>
#include <QThread>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rnqnx::runtime {
QtWorkletsScheduler::QtWorkletsScheduler(std::function<void(std::string)> reportError)
    : context_(std::make_unique<QObject>()), reportError_(std::move(reportError)) {
  const auto* app = QCoreApplication::instance();
  if (!app || QThread::currentThread() != app->thread()) {
    throw std::logic_error("Worklets scheduler requires the Qt application owner");
  }
}
QtWorkletsScheduler::~QtWorkletsScheduler() { close(); }
bool QtWorkletsScheduler::queryIsOnUIThread() const {
  return QThread::currentThread() == QCoreApplication::instance()->thread();
}
void QtWorkletsScheduler::scheduleOnUI(std::function<void()> job) {
  std::lock_guard lock(admission_);
  if (closed_) return;
  worklets::UIScheduler::scheduleOnUI(std::move(job));
  if (scheduledOnUI_.exchange(true)) return;
  const auto weak = weak_from_this();
  QMetaObject::invokeMethod(context_.get(), [weak] {
    if (const auto scheduler = weak.lock()) scheduler->drain();
  }, Qt::QueuedConnection);
}
void QtWorkletsScheduler::drain() {
  if (!queryIsOnUIThread()) std::terminate();
  // The upstream queue is consumed only on this owner. No admission lock is
  // held while executing JS or error handlers, which can schedule more jobs.
  const auto reportError = reportError_;
  scheduledOnUI_ = false;
  while (!uiJobs_.empty()) {
    auto job = uiJobs_.pop();
    try { job(); }
    catch (const std::exception& error) { reportError(error.what()); }
    catch (...) { reportError("Unknown Worklets UI runtime exception"); }
  }
}
void QtWorkletsScheduler::close() {
  std::vector<std::function<void()>> discarded;
  {
    std::lock_guard lock(admission_);
    if (closed_) return;
    if (!queryIsOnUIThread()) std::terminate();
    closed_ = true;
    context_.reset();
    while (!uiJobs_.empty()) discarded.push_back(uiJobs_.pop());
  }
  // No lock is held while releasing worklet captures, which may own resources
  // with reentrant shutdown callbacks. All destruction remains on the owner.
  discarded.clear();
  reportError_ = {};
}
}
