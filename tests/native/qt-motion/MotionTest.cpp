#include "MotionController.h"

#include <QEventLoop>
#include <QGuiApplication>
#include <QQuickItem>
#include <QThread>
#include <QTimer>

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace rnqnx::motion;

namespace {

void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

bool near(double left, double right) { return std::abs(left - right) < 0.0001; }

template <class Callback>
void rejects(Callback callback, const char* message) {
  bool threw = false;
  try { callback(); } catch (const std::exception&) { threw = true; }
  require(threw, message);
}

struct Terminal {
  quint64 operation;
  Track track;
  Completion completion;
};

struct RunningThread {
  QThread thread;
  RunningThread() { thread.start(); }
  ~RunningThread() { thread.quit(); thread.wait(); }
};

void trackProgressAndInterruption() {
  QQuickItem item;
  item.setPosition({30, 40});
  item.setSize({200, 100});
  MotionController motion(item);
  std::vector<Terminal> terminals;
  QObject::connect(&motion, &MotionController::completed, &motion,
      [&](quint64 id, Track track, Completion completion) {
        terminals.push_back({id, track, completion});
      });
  const auto first = motion.animateOpacity(0.0, {1000, Easing::Linear});
  motion.seekTo(Track::Opacity, 250);
  require(near(item.opacity(), 0.75), "opacity did not reach actual intermediate value");
  const auto second = motion.animateOpacity(0.25, {1000, Easing::Linear});
  require(terminals.size() == 1 && terminals[0].operation == first &&
          terminals[0].completion == Completion::Interrupted, "replacement did not interrupt once");
  require(near(item.opacity(), 0.75), "replacement jumped from current opacity");
  motion.seekTo(Track::Opacity, 500);
  require(near(item.opacity(), 0.5), "replacement did not start at current opacity");

  Transform destination;
  destination.translateX = 100;
  destination.translateY = 40;
  const auto transformed = motion.animateTransform(destination, {1000, Easing::Linear});
  motion.seekTo(Track::Transform, 500);
  const QPointF presented = item.mapToItem(nullptr, {0, 0});
  require(near(presented.x(), 80) && near(presented.y(), 60), "transform did not affect presented coordinates");
  require(near(item.x(), 30) && near(item.y(), 40) && near(item.width(), 200) &&
          near(item.height(), 100), "motion changed Yoga geometry");
  require(near(item.opacity(), 0.5), "transform overwrote simultaneous opacity");
  motion.pause(Track::Transform);
  require(motion.state(Track::Transform) == State::Paused, "pause state missing");
  motion.seekTo(Track::Transform, 750);
  require(near(motion.transform().translateX, 75), "paused transform scrub failed");
  motion.resume(Track::Transform);
  require(motion.state(Track::Transform) == State::Running, "resume state missing");
  motion.seekTo(Track::Transform, 1000);
  motion.seekTo(Track::Opacity, 1000);
  require(terminals.size() == 3 && terminals[1].operation == transformed &&
          terminals[1].completion == Completion::Finished && terminals[2].operation == second &&
          terminals[2].completion == Completion::Finished, "finished tracks were not completed once");
  motion.cancel(Track::Opacity);
  motion.cancel(Track::Transform);
  require(terminals.size() == 3, "idle cancel emitted duplicate completion");
  motion.detach();
  const auto restored = item.mapToItem(nullptr, {0, 0});
  require(near(restored.x(), 30) && near(restored.y(), 40) &&
          near(motion.transform().translateX, 0), "detach retained the added transform");
}

void cancellationAndValidation() {
  QQuickItem item;
  MotionController motion(item);
  int completions = 0;
  Completion reason = Completion::Finished;
  QObject::connect(&motion, &MotionController::completed, &motion,
      [&](quint64, Track, Completion value) { ++completions; reason = value; });
  motion.animateOpacity(0, {1000, Easing::InQuad});
  motion.seekTo(Track::Opacity, 500);
  require(near(item.opacity(), 0.75), "Qt easing was not applied");
  rejects([&] { motion.animateOpacity(std::numeric_limits<double>::quiet_NaN()); }, "NaN opacity accepted");
  rejects([&] { motion.animateOpacity(1.1); }, "invalid opacity accepted");
  rejects([&] { motion.animateOpacity(1, {-1, Easing::Linear}); }, "negative duration accepted");
  rejects([&] { motion.animateOpacity(1, {20, static_cast<Easing>(99)}); }, "unknown easing accepted");
  Transform invalid;
  invalid.translateX = std::numeric_limits<double>::infinity();
  rejects([&] { motion.animateTransform(invalid); }, "infinite transform accepted");
  rejects([&] { motion.seekTo(Track::Opacity, 1001); }, "seek beyond duration accepted");
  rejects([&] { motion.cancel(static_cast<Track>(99)); }, "unknown track accepted");
  require(completions == 0 && near(item.opacity(), 0.75) &&
          motion.state(Track::Opacity) == State::Running, "invalid input changed existing animation");
  motion.cancel(Track::Opacity);
  motion.cancel(Track::Opacity);
  require(completions == 1 && reason == Completion::Cancelled && near(item.opacity(), 0.75),
          "cancel reset the current value or emitted twice");
  rejects([&] { motion.seekTo(Track::Opacity, 100); }, "inactive seek accepted");
  motion.animateOpacity(0.4, {0, Easing::Linear});
  require(near(item.opacity(), 0.4) && completions == 2 && reason == Completion::Finished,
          "zero duration did not apply and finish once");
  motion.animateOpacity(0.8, {1000, Easing::Linear});
  motion.pause(Track::Opacity);
  motion.seekTo(Track::Opacity, 1000);
  require(near(item.opacity(), 0.8) && completions == 3 &&
          motion.state(Track::Opacity) == State::Idle,
          "seeking a paused track to its endpoint did not finish");
}

void transformCompositionAndDestruction() {
  QQuickItem item;
  auto motion = std::make_unique<MotionController>(item);
  Transform destination;
  destination.translateX = 20;
  destination.translateY = 10;
  destination.scaleX = 2;
  destination.scaleY = 0.5;
  destination.rotationDegrees = 90;
  destination.originX = 10;
  destination.originY = 20;
  motion->animateTransform(destination, {1000, Easing::Linear});
  motion->seekTo(Track::Transform, 1000);
  const auto pivot = item.mapToItem(nullptr, {10, 20});
  const auto rotated = item.mapToItem(nullptr, {11, 20});
  require(near(pivot.x(), 30) && near(pivot.y(), 30) &&
          near(rotated.x(), 30) && near(rotated.y(), 32),
          "scale/rotation/origin composition is incorrect");
  item.setScale(1.5);
  item.setRotation(25);
  motion->animateTransform({}, {1000, Easing::Linear});
  motion->seekTo(Track::Transform, 500);
  require(near(item.scale(), 1.5) && near(item.rotation(), 25),
          "overlay changed the item's preexisting transform");
  motion.reset();
  auto transforms = item.transform();
  require(transforms.count && transforms.count(&transforms) == 0,
          "controller destruction retained its transform");
}

void lifetimeAndOwnership() {
  auto item = std::make_unique<QQuickItem>();
  MotionController motion(*item);
  rejects([&] { MotionController duplicate(*item); }, "two controllers owned one item");
  std::vector<Completion> completed;
  QObject::connect(&motion, &MotionController::completed, &motion,
      [&](quint64, Track, Completion reason) { completed.push_back(reason); });
  motion.animateOpacity(0, {1000, Easing::Linear});
  Transform transform;
  transform.scaleX = 2;
  motion.animateTransform(transform, {1000, Easing::Linear});
  item.reset();
  require(completed.size() == 2 && completed[0] == Completion::TargetDestroyed &&
          completed[1] == Completion::TargetDestroyed, "target destruction did not cancel both tracks");
  require(motion.state(Track::Opacity) == State::Detached, "destroyed target retained active state");
  rejects([&] { motion.animateOpacity(1); }, "destroyed target accepted a new animation");
  motion.detach();
  require(completed.size() == 2, "repeated detach duplicated terminal events");

  QQuickItem parent;
  QQuickItem child;
  child.setParentItem(&parent);
  MotionController attached(child);
  int detached = 0;
  QObject::connect(&attached, &MotionController::completed, &attached,
      [&](quint64, Track, Completion reason) { if (reason == Completion::Detached) ++detached; });
  attached.animateOpacity(0);
  child.setParentItem(nullptr);
  require(detached == 1 && attached.state(Track::Opacity) == State::Detached,
          "unmount did not detach the controller");
  MotionController replacement(child);
  replacement.animateOpacity(0.8, {0, Easing::Linear});
  require(near(child.opacity(), 0.8), "detached controller retained ownership");

  bool rejectedWrongThread = false;
  std::thread worker([&] {
    try { replacement.animateOpacity(0); }
    catch (const std::logic_error&) { rejectedWrongThread = true; }
  });
  worker.join();
  require(rejectedWrongThread, "foreign thread mutated the target");
}

void reentrantReplacement() {
  QQuickItem item;
  MotionController motion(item);
  const auto first = motion.animateOpacity(0, {1000, Easing::Linear});
  std::vector<Terminal> terminals;
  quint64 newest = 0;
  QObject::connect(&motion, &MotionController::completed, &motion,
      [&](quint64 operation, Track track, Completion completion) {
        terminals.push_back({operation, track, completion});
        if (operation == first) newest = motion.animateOpacity(0.6, {1000, Easing::Linear});
      });
  const auto superseded = motion.animateOpacity(0.2, {1000, Easing::Linear});
  require(newest != 0 && terminals.size() == 2 && terminals[1].operation == superseded &&
          terminals[1].completion == Completion::Interrupted,
          "reentrant replacement lost a terminal operation");
  motion.seekTo(Track::Opacity, 1000);
  require(near(item.opacity(), 0.6) && terminals.size() == 3 &&
          terminals.back().operation == newest, "outer start overrode the newer native command");
}

void propertySignalReplacement() {
  // An opacityChanged listener runs inside QVariantAnimation's time update.
  // Replacing at its endpoint must not let that old clock finish the new run.
  for (const int firstDuration : {1000, 0}) {
    QQuickItem item;
    MotionController motion(item);
    std::vector<Terminal> terminals;
    quint64 replacement = 0;
    bool reentered = false;
    QObject::connect(&motion, &MotionController::completed, &motion,
        [&](quint64 operation, Track track, Completion completion) {
          terminals.push_back({operation, track, completion});
        });
    QObject::connect(&item, &QQuickItem::opacityChanged, &motion, [&] {
      if (!reentered && near(item.opacity(), 0)) {
        reentered = true;
        replacement = motion.animateOpacity(1, {1000, Easing::Linear});
      }
    });
    const auto first = motion.animateOpacity(0, {firstDuration, Easing::Linear});
    if (firstDuration) motion.seekTo(Track::Opacity, firstDuration);
    require(replacement != 0 && motion.state(Track::Opacity) == State::Running &&
            near(item.opacity(), 0) && terminals.size() == 1 && terminals[0].operation == first,
            "property callback finished its replacement prematurely");
    require(terminals[0].completion == (firstDuration ? Completion::Interrupted : Completion::Finished),
            "property replacement lost the previous operation's terminal reason");
    motion.seekTo(Track::Opacity, 500);
    require(near(item.opacity(), 0.5), "property replacement lost its starting value");
    motion.seekTo(Track::Opacity, 1000);
    require(near(item.opacity(), 1) && terminals.size() == 2 &&
            terminals[1].operation == replacement && terminals[1].completion == Completion::Finished,
            "property replacement did not finish exactly once at its own endpoint");
  }
}

void affinityChangesDetach() {
  RunningThread worker;
  QThread* const guiThread = QThread::currentThread();
  QQuickItem item;
  MotionController motion(item);
  std::vector<Completion> terminals;
  QObject::connect(&motion, &MotionController::completed, &motion,
      [&](quint64, Track, Completion reason) { terminals.push_back(reason); });
  motion.animateOpacity(0, {1000, Easing::Linear});
  motion.seekTo(Track::Opacity, 500);
  Transform transform;
  transform.translateX = 10;
  motion.animateTransform(transform, {1000, Easing::Linear});
  const bool itemMoved = item.moveToThread(&worker.thread);
  rejects([&] { motion.animateOpacity(0); }, "moved target accepted a motion command");
  bool itemReturned = false;
  if (itemMoved) {
    QMetaObject::invokeMethod(&item, [&] { itemReturned = item.moveToThread(guiThread); },
                              Qt::BlockingQueuedConnection);
  }
  require(itemMoved && itemReturned, "target affinity test could not move its object");
  require(motion.state(Track::Opacity) == State::Detached &&
          terminals.size() == 2 && terminals[0] == Completion::Detached &&
          terminals[1] == Completion::Detached && near(item.opacity(), 0.5),
          "target affinity change did not safely detach both tracks");
  require(near(item.mapToItem(nullptr, {0, 0}).x(), 0), "affinity change retained its transform");

  MotionController replacement(item);
  int controllerDetached = 0;
  QObject::connect(&replacement, &MotionController::completed, &item,
      [&](quint64, Track, Completion reason) {
        if (reason == Completion::Detached) ++controllerDetached;
      });
  replacement.animateOpacity(0, {1000, Easing::Linear});
  const bool controllerMoved = replacement.moveToThread(&worker.thread);
  rejects([&] { replacement.animateOpacity(1); }, "moved controller accepted a GUI call");
  bool controllerReturned = false;
  if (controllerMoved) {
    QMetaObject::invokeMethod(&replacement, [&] {
      controllerReturned = replacement.moveToThread(guiThread);
    }, Qt::BlockingQueuedConnection);
  }
  require(controllerMoved && controllerReturned, "controller affinity test could not move its object");
  require(replacement.state(Track::Opacity) == State::Detached && controllerDetached == 1,
          "controller affinity change did not permanently detach");
  rejects([&] { replacement.animateOpacity(1); }, "returned controller silently reattached");
}

void realClockProgress() {
  QQuickItem item;
  MotionController motion(item);
  QEventLoop loop;
  QTimer deadline;
  deadline.setSingleShot(true);
  bool timedOut = false;
  bool intermediate = false;
  int finished = 0;
  QObject::connect(&item, &QQuickItem::opacityChanged, &loop, [&] {
    intermediate = intermediate || (item.opacity() > 0.05 && item.opacity() < 0.95);
  });
  QObject::connect(&deadline, &QTimer::timeout, &loop, [&] { timedOut = true; loop.quit(); });
  QObject::connect(&motion, &MotionController::completed, &loop,
      [&](quint64, Track, Completion reason) {
        if (reason == Completion::Finished) ++finished;
        loop.quit();
      });
  motion.animateOpacity(0, {120, Easing::Linear});
  deadline.start(3000);
  loop.exec();
  require(!timedOut && intermediate && finished == 1 && near(item.opacity(), 0),
          "Qt animation clock failed actual timed progression");
}

} // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    trackProgressAndInterruption();
    cancellationAndValidation();
    transformCompositionAndDestruction();
    lifetimeAndOwnership();
    reentrantReplacement();
    propertySignalReplacement();
    affinityChangesDetach();
    realClockProgress();
    std::cout << "RNQNX_QT_MOTION_OK progress easing interruption pause resume cancel lifetime ownership reentrancy affinity clock\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_QT_MOTION_FAILED: " << error.what() << '\n';
    return 1;
  }
}
