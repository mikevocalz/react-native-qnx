# Animate native Qt items

The v2 motion slice provides a C++ `MotionController` for opacity and an added
2D transform on an existing `QQuickItem`. It supports timed easing, interruption,
cancel, pause, resume and seeking. The React Native renderer continues to own
layout. This slice does not implement React Native Worklets, Reanimated or
Gesture Handler, and the TypeScript contract below is a proposal, not an
installed JavaScript API.

Create, use and destroy the controller on the item's GUI owner thread. A target
can have one controller. That controller can animate opacity and transforms at
the same time. A second controller is rejected until the first detaches.

## Animate and interrupt a view

Link `rnqnx_qt_motion` and include `MotionController.h`:

```cpp
using namespace rnqnx::motion;

MotionController motion(card);
QObject::connect(&motion, &MotionController::completed, &motion,
    [](quint64 operation, Track track, Completion reason) {
      // Record the native operation's terminal result.
    });

motion.animateOpacity(0.4, {180, Easing::OutQuad});
Transform destination;
destination.translateX = 120;
destination.scaleX = 0.96;
destination.scaleY = 0.96;
destination.originX = card.width() / 2;
destination.originY = card.height() / 2;
motion.animateTransform(destination, {240, Easing::InOutCubic});

// A later command starts from the value being displayed.
destination.translateX = 0;
motion.animateTransform(destination, {160, Easing::OutQuad});
```

Every accepted operation receives a monotonically increasing native identifier.
A replacement completes the earlier operation with `Interrupted`. The two
tracks have independent identifiers and completions. Native terminal listeners
can replace or cancel work; a replacement made inside a listener takes
precedence over the earlier command. The same rule applies to a listener on the
target's `opacityChanged` signal. Each operation has a distinct Qt animation
object, so a previous clock cannot complete a replacement.

`Transform` adds translation, scale and rotation around a local origin. It does
not change the item's `x`, `y`, `width`, `height`, existing `scale`, or existing
`rotation`. Positions and origins use logical pixels; rotation uses degrees.
The transform order is translation, origin offset, rotation, scale, then inverse
origin offset. Negative scales mirror the item. Rotation follows the supplied
angle numerically; it does not select a shorter angular path.

## Pause, scrub and clean up

```cpp
motion.pause(Track::Transform);
motion.seekTo(Track::Transform, 80); // elapsed milliseconds in the active run
motion.resume(Track::Transform);
motion.cancel(Track::Opacity);     // retain the displayed opacity

// Before releasing an owner that needs terminal notifications:
motion.detach();
```

Cancel is idempotent and retains the current value. Pause and resume do nothing
when the track is already in the requested state or is idle. Seeking requires
an active or paused track and a time between zero and its duration. Seeking to
the end completes the operation. A zero duration applies the endpoint and
completes synchronously.

Detach stops both tracks, reports `Detached` for active operations, removes
the added transform and releases ownership of the item. It retains the current
opacity. Repeated detach is safe. An item losing its parent triggers the same
cleanup. Moving either the target or controller to a different QObject thread
also detaches permanently, before its affinity changes; moving it back does not
reattach it. Destroying a standalone target reports `TargetDestroyed`. Destruction
of the controller itself releases resources without invoking consumers. Call
`detach()` first when your owner needs terminal notifications before teardown.

Completion signals execute on the native owner thread. They do not enter a
JavaScript runtime. A future binding must use the actual runtime's owned
dispatcher and must stop delivery before that runtime is destroyed.

## Handle invalid input

Invalid requests throw a C++ exception before changing an existing operation:

- Opacity must be finite and between 0 and 1.
- Duration must be non-negative and fit the `int` millisecond field.
- Easing is `Linear`, `InQuad`, `OutQuad`, or `InOutCubic`.
- Coordinates and origins must be finite and within ±1,000,000 logical pixels.
- Scales must be finite and within ±10,000; rotation within ±360,000 degrees.
- Calls from another thread, a second owner, and starts after detach fail.

These transform bounds keep the added float matrix within a usable numeric
range. They are explicit limits of this slice. Required unsupported options do
not silently become no-ops. The native library does not arbitrate writes from
external Qt animations or Fabric opacity updates; the runtime integration must
give each animated property one authority.

## Proposed TypeScript application contract

The following shape is an API design target for a generated Nitro v1 binding.
It is not exported, typechecked against a generated binding, or executable in
the QNX app. Returned animation handles, completion Promises and disposal need
real generator and runtime support before this becomes a public API.

```ts
type MotionOutcome = 'finished' | 'cancelled' | 'interrupted' | 'detached' | 'target-destroyed';

interface Animation {
  readonly finished: Promise<MotionOutcome>;
  pause(): void;
  resume(): void;
  seekTo(elapsedMs: number): void;
  cancel(): void;
}

interface MotionTiming {
  durationMs: number;
  easing: 'linear' | 'in-quad' | 'out-quad' | 'in-out-cubic';
}

interface MotionTransform {
  translateX: number;
  translateY: number;
  scaleX: number;
  scaleY: number;
  rotationDegrees: number;
  originX: number;
  originY: number;
}

interface ViewMotion {
  animateOpacity(to: number, timing: MotionTiming): Animation;
  animateTransform(to: MotionTransform, timing: MotionTiming): Animation;
  detach(): void;
}
```

Proposed fade call site, including failure and cleanup:

```ts
const motion = await nativeMotion.attach(cardRef);
try {
  const fade = motion.animateOpacity(0.4, { durationMs: 180, easing: 'out-quad' });
  const outcome = await fade.finished;
  if (outcome === 'finished') showDetails();
} catch (error) {
  reportMotionError(error);
} finally {
  motion.detach();
}
```

Proposed interrupted drawer call site:

```ts
const motion = await nativeMotion.attach(drawerRef);
try {
  const opened = motion.animateTransform(
    {
      translateX: 280,
      translateY: 0,
      scaleX: 1,
      scaleY: 1,
      rotationDegrees: 0,
      originX: 0,
      originY: 0,
    },
    { durationMs: 240, easing: 'in-out-cubic' }
  );
  // On a later close interaction, the same native track starts at its
  // displayed position and opened.finished resolves to 'interrupted'.
  const closed = motion.animateTransform(
    {
      translateX: 0,
      translateY: 0,
      scaleX: 1,
      scaleY: 1,
      rotationDegrees: 0,
      originX: 0,
      originY: 0,
    },
    { durationMs: 180, easing: 'out-quad' }
  );
  await closed.finished;
  await opened.finished;
} finally {
  motion.detach();
}
```

The attachment factory and view-reference resolution are part of that proposed
binding, not this C++ library. The eventual API must specify whether commands
remain on the GUI owner or cross to it asynchronously when RN's JS thread is
separated. It must preserve the cleanup behavior rather than hiding thread hops
behind synchronous accessors.

## Run the native checks

Use a Qt 6.11.1 or newer development SDK containing Core, Gui and Quick:

```sh
cmake -S tests/native/qt-motion -B build/qt-motion -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/qt/6.11.1/gcc_64 \
  -DCMAKE_BUILD_TYPE=Debug -DRNQNX_ENABLE_QT_MOTION=ON \
  -DRNQNX_MOTION_SANITIZERS=ON
cmake --build build/qt-motion
ctest --test-dir build/qt-motion --output-on-failure
```

The native test checks actual item opacity and transformed coordinates,
unchanged layout geometry, Qt easing, concurrent tracks, interruption,
cancellation, pause/resume, seeking, invalid input, duplicate ownership,
unmount/destruction, property-signal reentrancy, changed-affinity cleanup,
wrong-thread rejection and advancement by Qt's animation clock. Deterministic
seeks check intermediate values without timing assumptions;
a separate bounded event-loop test checks timed progression.

Host results do not establish a QNX cross-build, physical interaction,
presentation latency, a refresh-rate guarantee, or React Native package
compatibility. Qt's GUI animation work can still be delayed by a busy GUI
thread. Layout animations, springs, decay, arbitrary worklets and gesture
recognizers remain separate integration work.

Source references: [Qt animation framework](https://doc.qt.io/qt-6/animation-overview.html),
[QVariantAnimation](https://doc.qt.io/qt-6/qvariantanimation.html), and
[QAbstractAnimation](https://doc.qt.io/qt-6/qabstractanimation.html).
