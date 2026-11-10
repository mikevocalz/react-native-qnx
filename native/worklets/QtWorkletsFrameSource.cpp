#include "QtWorkletsFrameSource.h"
#include <QCoreApplication>
#include <QThread>
#include <chrono>
#include <stdexcept>
#include <utility>
namespace rnqnx::runtime {
QtWorkletsFrameSource::QtWorkletsFrameSource(std::shared_ptr<QtWorkletsScheduler> scheduler)
    : scheduler_(std::move(scheduler)) { requireOwner(); }
QtWorkletsFrameSource::~QtWorkletsFrameSource() { close(); }
void QtWorkletsFrameSource::requireOwner() const {
  if (!QCoreApplication::instance() || QThread::currentThread() != QCoreApplication::instance()->thread())
    throw std::logic_error("Worklets frames require the Qt owner thread");
}
void QtWorkletsFrameSource::attach(QQuickWindow* window) {
  requireOwner();
  if (closed_ || !window || window->thread() != QThread::currentThread())
    throw std::logic_error("Attach a live owner-thread Worklets window before loading the bundle");
  if (attached_) {
    if (window_ == window) return;
    throw std::logic_error("Worklets window replacement requires a new host");
  }
  attached_ = true;
  window_ = window;
  const auto weak = weak_from_this();
  const auto scheduler = std::weak_ptr<QtWorkletsScheduler>(scheduler_);
  swapped_ = QObject::connect(window, &QQuickWindow::frameSwapped, window,
      [weak, scheduler] {
        // Render-thread callback: no window access or JSI. Same steady-clock
        // millisecond basis as upstream Worklets performance.now().
        const auto now = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (const auto owner = scheduler.lock()) owner->scheduleOnUI([weak, now] {
          if (const auto source = weak.lock()) source->deliver(now);
        });
      }, Qt::DirectConnection);
  destroyed_ = QObject::connect(window, &QObject::destroyed, window, [weak] {
    if (const auto source = weak.lock()) source->close();
  }, Qt::DirectConnection);
}
bool QtWorkletsFrameSource::ready() const {
  requireOwner();
  return !closed_ && !window_.isNull();
}
void QtWorkletsFrameSource::request(std::function<void(double)> callback) {
  requireOwner();
  if (closed_ || !window_) throw std::runtime_error("Worklets frame source requires an attached live QQuickWindow");
  pending_.push_back(std::move(callback));
  window_->update();
}
void QtWorkletsFrameSource::deliver(double timestamp) {
  requireOwner();
  if (closed_) return;
  auto callbacks = std::move(pending_);
  pending_.clear();
  for (auto& callback : callbacks) {
    if (closed_) break;
    callback(timestamp);
  }
}
void QtWorkletsFrameSource::close() {
  if (closed_) return;
  requireOwner();
  closed_ = true;
  QObject::disconnect(swapped_);
  QObject::disconnect(destroyed_);
  window_.clear();
  pending_.clear();
}
}
