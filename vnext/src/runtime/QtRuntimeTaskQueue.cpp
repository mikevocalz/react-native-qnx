#include "react-native-qnx/runtime/QtRuntimeTaskQueue.h"

#include <QMetaObject>

#include <exception>
#include <stdexcept>
#include <utility>

namespace rnqnx::runtime {

QtRuntimeTaskQueue::QtRuntimeTaskQueue(
    Execute execute, Task enterJS, Task leaveJS)
    : execute_(std::move(execute)),
      enterJS_(std::move(enterJS)),
      leaveJS_(std::move(leaveJS)) {}

#if RNQNX_ENABLE_FRAME_TRACE
QtRuntimeTaskQueue::QtRuntimeTaskQueue(Execute execute, Task enterJS, Task leaveJS,
                                     std::shared_ptr<RuntimeTrace> trace)
    : QtRuntimeTaskQueue(std::move(execute), std::move(enterJS), std::move(leaveJS)) {
  trace_ = std::move(trace);
}
#endif

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
#if RNQNX_ENABLE_FRAME_TRACE
  std::uint64_t traceCorrelation = 0;
  if (trace_) {
    traceCorrelation = trace_->nextId();
    task = [trace = trace_, original = std::move(task), traceCorrelation]() mutable {
      trace->record(TraceStage::QueueExecute, TracePhase::Begin, traceCorrelation);
      struct End {
        std::shared_ptr<RuntimeTrace> trace;
        std::uint64_t correlation;
        int exceptions{std::uncaught_exceptions()};
        ~End() { trace->record(TraceStage::QueueExecute, TracePhase::End, correlation,
                               0, 0, std::uncaught_exceptions() > exceptions); }
      } end{trace, traceCorrelation};
      original();
    };
  }
#endif
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
#if RNQNX_ENABLE_FRAME_TRACE
  if (trace_) trace_->record(TraceStage::QueueAdmission, TracePhase::Instant, traceCorrelation);
#endif
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
#if RNQNX_ENABLE_FRAME_TRACE
  TraceSpan span(trace_, TraceStage::QueueClose);
#endif
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
