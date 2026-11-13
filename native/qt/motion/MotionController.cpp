#include "MotionController.h"
#include "MotionTransform.h"

#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QPointer>
#include <QQuickItem>
#include <QThread>
#include <QVariantAnimation>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rnqnx::motion {
namespace {

// QQuickItems and this registry are exclusively owned by the GUI thread.
QHash<QQuickItem*, QPointer<MotionController>>& owners() {
  static QHash<QQuickItem*, QPointer<MotionController>> registry;
  return registry;
}

QEasingCurve easingCurve(Easing easing) {
  switch (easing) {
    case Easing::Linear: return QEasingCurve(QEasingCurve::Linear);
    case Easing::InQuad: return QEasingCurve(QEasingCurve::InQuad);
    case Easing::OutQuad: return QEasingCurve(QEasingCurve::OutQuad);
    case Easing::InOutCubic: return QEasingCurve(QEasingCurve::InOutCubic);
  }
  throw std::invalid_argument("Unknown motion easing");
}

void validate(Timing timing) {
  if (timing.durationMs < 0) {
    throw std::invalid_argument("Motion durationMs must be non-negative");
  }
  (void)easingCurve(timing.easing);
}

void validate(const Transform& value) {
  for (double coordinate : {value.translateX, value.translateY,
                             value.originX, value.originY}) {
    if (!std::isfinite(coordinate) || std::abs(coordinate) > 1'000'000.0) {
      throw std::invalid_argument("Motion coordinates must be finite and within +/-1000000 logical pixels");
    }
  }
  for (double scale : {value.scaleX, value.scaleY}) {
    if (!std::isfinite(scale) || std::abs(scale) > 10'000.0) {
      throw std::invalid_argument("Motion scales must be finite and within +/-10000");
    }
  }
  if (!std::isfinite(value.rotationDegrees) || std::abs(value.rotationDegrees) > 360'000.0) {
    throw std::invalid_argument("Motion rotationDegrees must be finite and within +/-360000");
  }
}

Transform interpolate(const Transform& from, const Transform& to, double progress) {
  return {
      std::lerp(from.translateX, to.translateX, progress),
      std::lerp(from.translateY, to.translateY, progress),
      std::lerp(from.scaleX, to.scaleX, progress),
      std::lerp(from.scaleY, to.scaleY, progress),
      std::lerp(from.rotationDegrees, to.rotationDegrees, progress),
      std::lerp(from.originX, to.originX, progress),
      std::lerp(from.originY, to.originY, progress),
  };
}

void retireAnimation(QVariantAnimation* animation) {
  if (!animation) return;
  // A replacement can be requested synchronously from a target's property
  // signal. Its old animation may still be inside updateCurrentTime(). Keep
  // that QObject alive until its stack unwinds, with all callbacks detached.
  QObject::disconnect(animation, nullptr, nullptr, nullptr);
  animation->stop();
  animation->deleteLater();
}

} // namespace

struct MotionController::Impl {
  explicit Impl(QQuickItem& item)
      : target(&item), targetKey(&item), ownerThread(QThread::currentThread()),
        overlay(std::make_unique<MotionTransform>()), currentOpacity(item.opacity()) {}

  struct Run {
    QPointer<QVariantAnimation> animation;
    quint64 operation{0};
    double opacityEndpoint{1};
    Transform transformEndpoint;
  };

  QPointer<QQuickItem> target;
  QQuickItem* targetKey;
  QThread* const ownerThread;
  Run opacityRun;
  Run transformRun;
  std::unique_ptr<MotionTransform> overlay;
  QMetaObject::Connection destroyedConnection;
  QMetaObject::Connection parentConnection;
  quint64 sequence{0};
  double currentOpacity;
  Transform currentTransform;
  bool detached{false};

  Run& run(Track track) {
    switch (track) {
      case Track::Opacity: return opacityRun;
      case Track::Transform: return transformRun;
    }
    throw std::invalid_argument("Unknown motion track");
  }

  quint64 nextOperation() {
    if (sequence == std::numeric_limits<quint64>::max()) {
      throw std::overflow_error("Motion operation sequence exhausted");
    }
    return ++sequence;
  }
};

MotionController::MotionController(QQuickItem& target, QObject* parent) : QObject(parent) {
  auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
  if (!app || QThread::currentThread() != app->thread() ||
      target.thread() != QThread::currentThread() ||
      (parent && parent->thread() != QThread::currentThread())) {
    throw std::invalid_argument("Motion requires the QQuickItem GUI owner thread");
  }
  if (owners().contains(&target) && owners().value(&target)) {
    throw std::logic_error("A motion controller already owns this item; detach it first");
  }
  impl_ = std::make_unique<Impl>(target);
  owners().insert(&target, this);
  impl_->overlay->appendToItem(&target);
  target.installEventFilter(this);
  impl_->destroyedConnection = QObject::connect(&target, &QObject::destroyed, this,
      [this] { detachWithReason(Completion::TargetDestroyed, true); });
  impl_->parentConnection = QObject::connect(&target, &QQuickItem::parentChanged, this,
      [this](QQuickItem* next) { if (!next) detachWithReason(Completion::Detached, true); });
}

MotionController::~MotionController() {
  if (impl_) {
    Q_ASSERT_X(QThread::currentThread() == thread(),
               "MotionController", "Destroy on the QObject owner thread");
    detachWithReason(Completion::Detached, false);
  }
}

void MotionController::requireOwner() const {
  if (QThread::currentThread() != impl_->ownerThread || thread() != impl_->ownerThread ||
      (impl_->target && impl_->target->thread() != impl_->ownerThread)) {
    throw std::logic_error("Motion must be called on its GUI owner thread");
  }
}

bool MotionController::event(QEvent* event) {
  if (impl_ && event->type() == QEvent::ThreadChange) {
    QPointer<MotionController> alive(this);
    detachWithReason(Completion::Detached, true);
    if (!alive) return true;
  }
  return QObject::event(event);
}

bool MotionController::eventFilter(QObject* watched, QEvent* event) {
  if (impl_ && watched == impl_->targetKey && event->type() == QEvent::ThreadChange) {
    QPointer<MotionController> alive(this);
    QPointer<QObject> targetAlive(watched);
    detachWithReason(Completion::Detached, true);
    if (!alive || !targetAlive) return true;
  }
  return QObject::eventFilter(watched, event);
}

void MotionController::requireAttached() const {
  requireOwner();
  if (impl_->detached || !impl_->target) {
    throw std::logic_error("Motion target is detached or destroyed");
  }
}

quint64 MotionController::animateOpacity(double opacity, Timing timing) {
  requireAttached();
  validate(timing);
  if (!std::isfinite(opacity) || opacity < 0 || opacity > 1) {
    throw std::invalid_argument("Motion opacity must be finite and between 0 and 1");
  }
  return startRun(Track::Opacity, timing, opacity, {});
}

quint64 MotionController::animateTransform(const Transform& transform, Timing timing) {
  requireAttached();
  validate(timing);
  validate(transform);
  return startRun(Track::Transform, timing, 1, transform);
}

quint64 MotionController::startRun(Track track, Timing timing, double opacity,
                                  const Transform& transform) {
  const quint64 operation = impl_->nextOperation();
  const double fromOpacity = impl_->target->opacity();
  const Transform fromTransform = impl_->currentTransform;
  QPointer<QVariantAnimation> animation = new QVariantAnimation(this);
  animation->setStartValue(0.0);
  animation->setEndValue(1.0);
  animation->setDuration(timing.durationMs);
  animation->setEasingCurve(easingCurve(timing.easing));
  QObject::connect(animation, &QVariantAnimation::valueChanged, this,
      [this, track, operation, fromOpacity, opacity, fromTransform, transform](const QVariant& value) {
        if (impl_->detached || !impl_->target || impl_->run(track).operation != operation) return;
        const double progress = value.toDouble();
        if (track == Track::Opacity) {
          impl_->currentOpacity = std::lerp(fromOpacity, opacity, progress);
          impl_->target->setOpacity(impl_->currentOpacity);
        } else {
          impl_->currentTransform = interpolate(fromTransform, transform, progress);
          impl_->overlay->setValue(impl_->currentTransform);
        }
      });
  QObject::connect(animation, &QVariantAnimation::finished, this, [this, track, operation] {
    if (impl_->run(track).operation == operation) finish(track, Completion::Finished);
  });
  const auto previous = std::exchange(impl_->run(track),
      Impl::Run{animation, operation, opacity, transform});
  if (track == Track::Opacity) impl_->currentOpacity = fromOpacity;
  retireAnimation(previous.animation);
  QPointer<MotionController> alive(this);
  if (previous.operation) Q_EMIT completed(previous.operation, track, Completion::Interrupted);
  // A synchronous property or terminal listener may replace this operation.
  // Each run owns a distinct clock; an old clock can never finish a new run.
  if (alive && !impl_->detached && impl_->run(track).operation == operation && animation) {
    animation->start();
  }
  return operation;
}

void MotionController::finish(Track track, Completion reason) {
  const auto run = std::exchange(impl_->run(track), Impl::Run{});
  retireAnimation(run.animation);
  QPointer<MotionController> alive(this);
  if (run.operation && reason == Completion::Finished && !impl_->detached && impl_->target) {
    // Qt may finish a zero-duration run without valueChanged, and its final
    // value may equal the preceding run. Always commit the exact endpoint.
    if (track == Track::Opacity) {
      impl_->currentOpacity = run.opacityEndpoint;
      impl_->target->setOpacity(impl_->currentOpacity);
    } else {
      impl_->currentTransform = run.transformEndpoint;
      impl_->overlay->setValue(impl_->currentTransform);
    }
  }
  if (alive && run.operation) Q_EMIT completed(run.operation, track, reason);
}

void MotionController::cancel(Track track) {
  requireOwner();
  finish(track, Completion::Cancelled);
}

void MotionController::pause(Track track) {
  requireAttached();
  auto animation = impl_->run(track).animation;
  if (animation && animation->state() == QAbstractAnimation::Running) animation->pause();
}

void MotionController::resume(Track track) {
  requireAttached();
  auto animation = impl_->run(track).animation;
  if (animation && animation->state() == QAbstractAnimation::Paused) animation->resume();
}

void MotionController::seekTo(Track track, int elapsedMs) {
  requireAttached();
  auto animation = impl_->run(track).animation;
  if (!animation) throw std::logic_error("No active motion to seek");
  if (elapsedMs < 0 || elapsedMs > animation->duration()) {
    throw std::invalid_argument("Motion elapsedMs must be within its duration");
  }
  animation->setCurrentTime(elapsedMs);
}

void MotionController::detachWithReason(Completion reason, bool notify) {
  if (impl_->detached) return;
  impl_->detached = true;
  if (impl_->target) {
    impl_->currentOpacity = impl_->target->opacity();
    impl_->target->removeEventFilter(this);
  }
  owners().remove(impl_->targetKey);
  QObject::disconnect(impl_->destroyedConnection);
  QObject::disconnect(impl_->parentConnection);
  const auto opacityRun = std::exchange(impl_->opacityRun, Impl::Run{});
  const auto transformRun = std::exchange(impl_->transformRun, Impl::Run{});
  retireAnimation(opacityRun.animation);
  retireAnimation(transformRun.animation);
  impl_->target.clear();
  impl_->overlay.reset();
  impl_->currentTransform = {};
  QPointer<MotionController> alive(this);
  if (notify && opacityRun.operation) Q_EMIT completed(opacityRun.operation, Track::Opacity, reason);
  if (alive && notify && transformRun.operation) Q_EMIT completed(transformRun.operation, Track::Transform, reason);
}

void MotionController::detach() {
  requireOwner();
  detachWithReason(Completion::Detached, true);
}

State MotionController::state(Track track) const {
  requireOwner();
  const auto& run = impl_->run(track);
  if (impl_->detached) return State::Detached;
  if (!run.operation) return State::Idle;
  return run.animation && run.animation->state() == QAbstractAnimation::Paused ? State::Paused : State::Running;
}

double MotionController::opacity() const {
  requireOwner();
  return impl_->target ? impl_->target->opacity() : impl_->currentOpacity;
}

Transform MotionController::transform() const {
  requireOwner();
  return impl_->currentTransform;
}

} // namespace rnqnx::motion
