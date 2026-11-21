#include "react-native-qnx/runtime/QueuedCallTarget.h"

#include <QCoreApplication>
#include <QEvent>

#include <utility>

#if defined(__SANITIZE_THREAD__)
#define RNQNX_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define RNQNX_TSAN 1
#endif
#endif
#ifdef RNQNX_TSAN
#include <sanitizer/tsan_interface.h>
#endif

namespace rnqnx::runtime {

namespace {

QEvent::Type callEventType() {
  static const auto type = static_cast<QEvent::Type>(QEvent::registerEventType());
  return type;
}

class CallEvent final : public QEvent {
 public:
  explicit CallEvent(std::function<void()> work)
      : QEvent(callEventType()), work(std::move(work)) {}
  std::function<void()> work;
};

}  // namespace

void QueuedCallTarget::postCall(std::function<void()> work) {
  auto* call = new CallEvent(std::move(work));
#ifdef RNQNX_TSAN
  __tsan_release(call);
#endif
  QCoreApplication::postEvent(this, call);
}

bool QueuedCallTarget::event(QEvent* event) {
  if (event->type() != callEventType()) {
    return QObject::event(event);
  }
  auto* call = static_cast<CallEvent*>(event);
#ifdef RNQNX_TSAN
  __tsan_acquire(call);
#endif
  call->work();
  return true;
}

}  // namespace rnqnx::runtime
