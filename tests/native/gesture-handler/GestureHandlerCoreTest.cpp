// Host test for the Gesture Handler recognizer registry. No Qt, no React Native:
// pointer input in, state transitions out, with a fake clock.
#include "react-native-qnx/gesture/GestureHandlerCore.h"
#include "react-native-qnx/gesture/GestureHandlerBindings.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace rnqnx::gesture;

namespace {

int failures = 0;
#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);        \
      ++failures;                                                                \
    }                                                                            \
  } while (0)

struct Rig {
  Registry reg;
  std::vector<GestureEvent> log;
  Rig() {
    reg.setSink([this](const GestureEvent& e) { log.push_back(e); });
    // Two views: 1 is a 400x400 card, 2 is a 100x100 square inside it.
    reg.setHitTest([](int view, double x, double y) {
      ViewHit hit;
      if (view == 1) hit = {x >= 0 && x < 400 && y >= 0 && y < 400, x, y};
      if (view == 2) hit = {x >= 100 && x < 200 && y >= 100 && y < 200, x - 100, y - 100};
      return hit;
    });
  }
  int count(int tag, State s, EventKind k = EventKind::StateChange) const {
    int n = 0;
    for (const auto& e : log) n += e.handlerTag == tag && e.state == s && e.kind == k;
    return n;
  }
};

void detectorBindingsHandlePassiveEffectOrdering() {
  for (bool mountFirst : {false, true}) {
    Rig r;
    DetectorBindings bindings(r.reg);
    if (mountFirst) bindings.observe(1, {7});
    bindings.create("PanGestureHandler", 7, {});
    if (!mountFirst) bindings.observe(1, {7});
    bindings.configure(7, {{"minDist", 5}, {"dispatchesReanimatedEvents", 1}});
    r.reg.pointerDown(0, 50, 50, 0);
    r.reg.pointerMove(0, 70, 50, 16);
    CHECK(r.count(7, State::Active) == 1);
    CHECK(!r.log.empty() && r.log.back().actionType == 1);
    r.reg.pointerUp(0, 70, 50, 32);
    bindings.configure(7, {{"dispatchesReanimatedEvents", 0}});
    r.reg.pointerDown(0, 50, 50, 40);
    CHECK(!r.log.empty() && r.log.back().actionType == 4);
    r.reg.pointerCancel(0, 45);
    bindings.removeView(1);
    const auto count = r.log.size();
    r.reg.pointerDown(0, 50, 50, 50);
    CHECK(r.log.size() == count);
    r.reg.pointerUp(0, 50, 50, 60);
    bindings.drop(7);
    bindings.create("PanGestureHandler", 7, {});
    r.reg.pointerDown(0, 50, 50, 70);
    CHECK(r.log.size() == count);
    r.reg.pointerUp(0, 50, 50, 80);
  }
}

void defaultExclusiveTapWaitsPastItsOwnDuration() {
  Rig r;
  r.reg.create("TapGestureHandler", 1, {});
  r.reg.create("TapGestureHandler", 2, {{"numberOfTaps", 2}});
  r.reg.attach(1, 1, 1);
  r.reg.attach(2, 1, 1);
  r.reg.setRelations(1, {{}, {2}, {}});
  for (double start : {0.0, 1000.0}) {
    r.reg.pointerDown(0, 10, 10, start);
    r.reg.pointerUp(0, 10, 10, start + 60);
    r.reg.advance(start + 501);
    CHECK(r.reg.stateOf(1) == State::Began);
    r.reg.advance(start + 561);
    CHECK(r.reg.stateOf(1) == State::Undetermined);
    CHECK(r.reg.stateOf(2) == State::Undetermined);
  }
  CHECK(r.count(1, State::End) == 2);
  CHECK(r.count(2, State::Active) == 0);
}

void panActivatesAfterMinDistance() {
  Rig r;
  r.reg.create("PanGestureHandler", 1, {{"minDist", 10}});
  r.reg.attach(1, 1, 1);
  r.reg.pointerDown(0, 50, 50, 0);
  CHECK(r.reg.stateOf(1) == State::Began);
  r.reg.pointerMove(0, 55, 50, 8);
  CHECK(r.reg.stateOf(1) == State::Began);
  r.reg.pointerMove(0, 70, 50, 16);
  CHECK(r.reg.stateOf(1) == State::Active);
  r.reg.pointerMove(0, 90, 50, 24);
  CHECK(r.count(1, State::Active, EventKind::Update) == 1);
  CHECK(r.log.back().data.translationX == 40);
  CHECK(r.log.back().data.velocityX > 0);
  r.reg.pointerUp(0, 90, 50, 32);
  CHECK(r.count(1, State::End) == 1);
  CHECK(r.reg.stateOf(1) == State::Undetermined);  // reset once idle
}

void panFailsOnFailOffset() {
  Rig r;
  r.reg.create("PanGestureHandler", 1, {{"activeOffsetXStart", -20}, {"activeOffsetXEnd", 20}, {"failOffsetYStart", -5}, {"failOffsetYEnd", 5}});
  r.reg.attach(1, 1, 1);
  r.reg.pointerDown(0, 50, 50, 0);
  r.reg.pointerMove(0, 52, 80, 8);
  CHECK(r.count(1, State::Failed) == 1);
  r.reg.pointerMove(0, 200, 80, 16);
  CHECK(r.count(1, State::Active) == 0);
  r.reg.pointerUp(0, 200, 80, 24);
}

void tapCompletes() {
  Rig r;
  r.reg.create("TapGestureHandler", 1, {});
  r.reg.attach(1, 1, 1);
  r.reg.pointerDown(0, 10, 10, 0);
  r.reg.pointerUp(0, 10, 10, 120);
  CHECK(r.count(1, State::Active) == 1);
  CHECK(r.count(1, State::End) == 1);
}

void tapTooSlowFails() {
  Rig r;
  r.reg.create("TapGestureHandler", 1, {{"maxDurationMs", 200}});
  r.reg.attach(1, 1, 1);
  r.reg.pointerDown(0, 10, 10, 0);
  r.reg.advance(250);
  CHECK(r.count(1, State::Failed) == 1);
  r.reg.pointerUp(0, 10, 10, 300);
  CHECK(r.count(1, State::Active) == 0);
}

void panBeatsTapExclusively() {
  Rig r;
  r.reg.create("TapGestureHandler", 1, {});
  r.reg.create("PanGestureHandler", 2, {{"minDist", 10}});
  r.reg.attach(1, 1, 1);
  r.reg.attach(2, 1, 1);
  r.reg.pointerDown(0, 50, 50, 0);
  r.reg.pointerMove(0, 80, 50, 16);
  CHECK(r.reg.stateOf(2) == State::Active);
  CHECK(r.count(1, State::Failed) == 1);  // tap lost to the pan
  r.reg.pointerUp(0, 80, 50, 32);
  CHECK(r.count(1, State::Active) == 0);
}

void simultaneousPansBothActivate() {
  Rig r;
  r.reg.create("PanGestureHandler", 1, {{"minDist", 10}});
  r.reg.create("PanGestureHandler", 2, {{"minDist", 10}});
  r.reg.attach(1, 1, 1);
  r.reg.attach(2, 1, 1);
  r.reg.setRelations(1, {{2}, {}, {}});
  r.reg.pointerDown(0, 50, 50, 0);
  r.reg.pointerMove(0, 80, 50, 16);
  CHECK(r.reg.stateOf(1) == State::Active);
  CHECK(r.reg.stateOf(2) == State::Active);
  r.reg.pointerUp(0, 80, 50, 32);
  CHECK(r.count(1, State::End) == 1 && r.count(2, State::End) == 1);
}

void waitForDelaysActivation() {
  // Single tap waits for the double tap to fail.
  Rig r;
  r.reg.create("TapGestureHandler", 1, {{"numberOfTaps", 1}});
  r.reg.create("TapGestureHandler", 2, {{"numberOfTaps", 2}, {"maxDelayMs", 300}});
  r.reg.attach(1, 1, 1);
  r.reg.attach(2, 1, 1);
  r.reg.setRelations(1, {{}, {2}, {}});
  r.reg.pointerDown(0, 10, 10, 0);
  r.reg.pointerUp(0, 10, 10, 50);
  CHECK(r.reg.stateOf(1) == State::Began);  // waiting
  CHECK(r.count(1, State::Active) == 0);
  r.reg.advance(400);  // the double tap's second-tap window closes
  CHECK(r.count(2, State::Failed) == 1);
  CHECK(r.count(1, State::Active) == 1);
  CHECK(r.count(1, State::End) == 1);
}

void doubleTapWinsOverWaiter() {
  Rig r;
  r.reg.create("TapGestureHandler", 1, {{"numberOfTaps", 1}});
  r.reg.create("TapGestureHandler", 2, {{"numberOfTaps", 2}, {"maxDelayMs", 300}});
  r.reg.attach(1, 1, 1);
  r.reg.attach(2, 1, 1);
  r.reg.setRelations(1, {{}, {2}, {}});
  r.reg.pointerDown(0, 10, 10, 0);
  r.reg.pointerUp(0, 10, 10, 50);
  r.reg.pointerDown(0, 10, 10, 120);
  r.reg.pointerUp(0, 10, 10, 160);
  CHECK(r.count(2, State::End) == 1);
  CHECK(r.count(1, State::Active) == 0);
  CHECK(r.count(1, State::Failed) == 1);
}

void longPressFiresOnDeadline() {
  Rig r;
  r.reg.create("LongPressGestureHandler", 1, {{"minDurationMs", 400}});
  r.reg.attach(1, 1, 1);
  r.reg.pointerDown(0, 10, 10, 0);
  CHECK(r.reg.nextDeadlineMs().value_or(-1) == 400);
  r.reg.advance(399);
  CHECK(r.reg.stateOf(1) == State::Began);
  r.reg.advance(400);
  CHECK(r.reg.stateOf(1) == State::Active);
  r.reg.pointerUp(0, 10, 10, 500);
  CHECK(r.count(1, State::End) == 1);
}

void flingNeedsVelocityAndDirection() {
  Rig r;
  r.reg.create("FlingGestureHandler", 1, {{"direction", 1}});  // right
  r.reg.attach(1, 1, 1);
  r.reg.pointerDown(0, 10, 10, 0);
  r.reg.pointerMove(0, 60, 10, 20);
  r.reg.pointerMove(0, 160, 10, 40);
  r.reg.pointerUp(0, 260, 10, 60);
  CHECK(r.count(1, State::End) == 1);
  CHECK(r.log.back().data.direction == 1);

  Rig left;
  left.reg.create("FlingGestureHandler", 1, {{"direction", 1}});
  left.reg.attach(1, 1, 1);
  left.reg.pointerDown(0, 300, 10, 0);
  left.reg.pointerMove(0, 200, 10, 20);
  left.reg.pointerUp(0, 100, 10, 40);
  CHECK(left.count(1, State::End) == 0);
  CHECK(left.count(1, State::Failed) == 1);
}

void hitTestPicksTheAttachedView() {
  Rig r;
  r.reg.create("TapGestureHandler", 1, {});
  r.reg.attach(1, 2, 1);  // the small square
  r.reg.pointerDown(0, 10, 10, 0);
  CHECK(r.reg.stateOf(1) == State::Undetermined);  // outside the square
  r.reg.pointerUp(0, 10, 10, 20);
  r.reg.pointerDown(0, 150, 150, 100);
  CHECK(r.reg.stateOf(1) == State::Began);
  r.reg.pointerUp(0, 150, 150, 120);
  CHECK(r.log.back().data.x == 50);
}

void cancelFailsActiveHandlers() {
  Rig r;
  r.reg.create("PanGestureHandler", 1, {{"minDist", 5}});
  r.reg.attach(1, 1, 1);
  r.reg.pointerDown(0, 50, 50, 0);
  r.reg.pointerMove(0, 80, 50, 16);
  CHECK(r.reg.stateOf(1) == State::Active);
  r.reg.pointerCancel(0, 20);
  CHECK(r.count(1, State::Cancelled) == 1);
  CHECK(!r.reg.hasActivePointers());
}

void manualHandlerFollowsStateManager() {
  Rig r;
  r.reg.create("ManualGestureHandler", 1, {});
  r.reg.attach(1, 1, 1);
  r.reg.pointerDown(0, 50, 50, 0);
  CHECK(r.reg.stateOf(1) == State::Began);
  r.reg.setState(1, State::Active);
  CHECK(r.reg.stateOf(1) == State::Active);
  r.reg.setState(1, State::End);
  CHECK(r.count(1, State::End) == 1);
  r.reg.pointerUp(0, 50, 50, 10);
}

void unsupportedHandlersFailAndAreListed() {
  Rig r;
  r.reg.create("PinchGestureHandler", 1, {});
  r.reg.attach(1, 1, 1);
  r.reg.pointerDown(0, 50, 50, 0);
  CHECK(r.count(1, State::Failed) == 1);
  CHECK(r.reg.unsupported().size() == 1);
  r.reg.pointerUp(0, 50, 50, 5);
}

}  // namespace

int main() {
  detectorBindingsHandlePassiveEffectOrdering();
  defaultExclusiveTapWaitsPastItsOwnDuration();
  panActivatesAfterMinDistance();
  panFailsOnFailOffset();
  tapCompletes();
  tapTooSlowFails();
  panBeatsTapExclusively();
  simultaneousPansBothActivate();
  waitForDelaysActivation();
  doubleTapWinsOverWaiter();
  longPressFiresOnDeadline();
  flingNeedsVelocityAndDirection();
  hitTestPicksTheAttachedView();
  cancelFailsActiveHandlers();
  manualHandlerFollowsStateManager();
  unsupportedHandlersFailAndAreListed();
  if (failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  std::printf("RNQNX_GESTURE_CORE_OK\n");
  return 0;
}
