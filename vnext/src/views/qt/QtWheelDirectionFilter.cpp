#include "react-native-qnx/views/QtWheelDirectionFilter.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QScopedValueRollback>
#include <QThread>
#include <QWheelEvent>

#include <limits>
#include <stdexcept>

namespace rnqnx::views {
namespace {

int invertDelta(int value) {
  // A malformed INT_MIN delta has no positive int counterpart. Saturate that
  // one value instead of overflowing; normal device deltas are exact negations.
  return value == (std::numeric_limits<int>::min)()
      ? (std::numeric_limits<int>::max)()
      : -value;
}

class InvertedWheelEvent final : public QWheelEvent {
 public:
  explicit InvertedWheelEvent(const QWheelEvent& original) : QWheelEvent(original) {
    // Copy the complete event, including event-point state, device, timestamp,
    // source, phase and inverted() metadata. Only the two deltas change.
    m_pixelDelta = QPoint(invertDelta(m_pixelDelta.x()), invertDelta(m_pixelDelta.y()));
    m_angleDelta = QPoint(invertDelta(m_angleDelta.x()), invertDelta(m_angleDelta.y()));
  }
};

}  // namespace

QtWheelDirectionFilter::QtWheelDirectionFilter(QQuickWindow& window, bool invert)
    : window_(invert ? &window : nullptr) {
  if (QThread::currentThread() != window.thread()) {
    throw std::logic_error("Wheel direction filter requires the window's Qt owner thread");
  }
  if (window_) {
    window_->installEventFilter(this);
  }
}

QtWheelDirectionFilter::~QtWheelDirectionFilter() {
  if (window_) {
    window_->removeEventFilter(this);
  }
}

bool QtWheelDirectionFilter::eventFilter(QObject* watched, QEvent* event) {
  if (watched != window_ || redispatching_ || event->type() != QEvent::Wheel) {
    return false;
  }

  InvertedWheelEvent inverted(*static_cast<QWheelEvent*>(event));
  const QScopedValueRollback<bool> guard(redispatching_, true);
  // We are already inside the GUI-thread notify stack. Re-enter the public
  // notify route so Qt's window delivery and other filters still run, without
  // sendEvent() clearing the original event's spontaneous flag.
  QCoreApplication::instance()->notify(watched, &inverted);
  event->setAccepted(inverted.isAccepted());
  return true;  // The original must not reach the window a second time.
}

}  // namespace rnqnx::views
