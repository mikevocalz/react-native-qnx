# Reanimated 4 and Gesture Handler 3 on QNX

Stage 3 of the [v2 plan](v2/README.md), on top of [Worklets](worklets-qnx.md).
Versions: `react-native-reanimated` 4.7.1 (requires Worklets 0.13.x, React
Native 0.86 to 0.88) and `react-native-gesture-handler` 3.4.0. Expo SDK 58's
`bundledNativeModules.json` selects RNGH `~3.3.0`, but the upstream
compatibility table lists 3.4.x for React Native 0.88 and 3.3.x without it, so
this work follows the table. `expo install` would pick the older line; pin
3.4.0 in the app. Upstream C++ is compiled from the installed packages and is
not vendored ([ADR-0020](adr/0020-worklets-reanimated-gesture-handler-bootstrap.md)).

## Enable

```sh
cmake ... -DRNQNX_ENABLE_WORKLETS=ON  -DRNQNX_WORKLETS_SOURCE_DIR=<node_modules>/react-native-worklets \
          -DRNQNX_ENABLE_REANIMATED=ON -DRNQNX_REANIMATED_SOURCE_DIR=<node_modules>/react-native-reanimated \
          -DRNQNX_ENABLE_GESTURE_HANDLER=ON -DRNQNX_GESTURE_HANDLER_SOURCE_DIR=<node_modules>/react-native-gesture-handler
```

All three default to OFF. Each reads its package version and fails the
configure step outside the supported range. The Fabric codegen the libraries'
headers need is generated into the build directory by
`scripts/fabric-codegen.mjs`, using the repository's pinned React Native
codegen (0.88.1).

## Reanimated

`QnxReanimatedModule` is the `ReanimatedModule` TurboModule. Its
`installTurboModule()` builds the upstream `ReanimatedModuleProxy` with these
platform methods:

| Method                                                     | Qt implementation                                                                            |
| ---------------------------------------------------------- | -------------------------------------------------------------------------------------------- |
| `requestRender`                                            | Qt Quick `afterAnimating`, via the Worklets frame bridge                                     |
| `getAnimationTimestamp`                                    | `steady_clock` milliseconds, same clock as frame timestamps                                  |
| `synchronouslyUpdateUIPropsFunction`                       | GUI-thread `schedulerShouldSynchronouslyUpdateViewOnUIThread` into the mounted item          |
| Fabric commit and mount hooks, measure, dispatchCommand    | The upstream implementation over the host's `UIManager`; mounting re-posts to the GUI thread |
| Events for `useAnimatedScrollHandler`, gesture handlers    | `Scheduler::addEventListener`; the handler worklet runs on the UI thread                     |
| Sensors, keyboard, pseudo-selectors, platform CSS backends | Not provided; sensors and keyboard log and return -1                                         |

Two upstream lines are text-patched in generated copies, and the configure step
fails if either changes: the `SynchronouslyUpdateUIPropsFunction` type (defined
only for Android and Apple) and the loop that applies synchronous props (Apple
only). `__APPLE__` and `ANDROID` are not defined.

## Gesture Handler

The recognizers are ours (`vnext/src/gesture/GestureHandlerCore.cpp`), a
registry with no Qt or React dependency and host tests for each rule. The
library's JavaScript and its detector shadow node are upstream's.

| Handler                        | Status                                                                                         |
| ------------------------------ | ---------------------------------------------------------------------------------------------- |
| Pan                            | Min distance, active and fail offsets, velocity, translation, begin/activate/update/end/cancel |
| Tap                            | Tap count, max duration, max delay between taps, max distance                                  |
| LongPress                      | Min duration deadline, max distance                                                            |
| Fling                          | Direction mask, minimum velocity                                                               |
| Manual                         | Driven by `GestureStateManager` through `_setGestureStateSync` and `_setGestureStateAsync`     |
| Pinch, Rotation, Native, Hover | Accepted and fail on the first pointer, with a log line. Not implemented.                      |

Arbitration: by default the first handler to activate fails every other
begun or active handler that shares a pointer. `simultaneousHandlers` lets two
handlers be active together. `waitFor` and `blocksHandlers` hold a handler at
Began until the other fails or ends. Input is Qt mouse and touch events,
observed by an event filter that never consumes them, so React responders and
`Pressable` keep working beside it.

Not implemented: pointer data callbacks (`onTouchesDown` and friends),
multi-pointer pan averaging, hit-slop, `RNGestureHandlerButton` behavior
(it mounts as a plain container), arbitration with Qt scrolling, and
coordinate spaces for transformed ancestors (the view's `mapFromScene` is
used).

## Thread authority and its cost

React Native JS runs on the Qt GUI thread on this host. Worklets, Reanimated
callbacks and gesture worklets run on the UI-worklet thread. When JavaScript
blocks the GUI thread, the screen cannot repaint, so a drag-and-spring under JS
load pauses on screen and then catches up from UI-thread state. Making that
smooth needs a dedicated JS thread with the GUI thread free to render, which is
the host change [Plan 01](platform-modernization/01-runtime-baseline.md)
describes and this work does not make.

## Continuation (Pi 400, 2026-11-13)

The Advanced tab and Reanimated card now run under the guarded showcase
launcher. Pointer clicks reach its spring/timing controls. Shared x advances
on the UI runtime, but the flush-only candidate updates pixels at a subsequent
React commit rather than each frame. The missing Fabric mount-completion
notification is under test. These observations supersede the earlier statement
that the cards could not be exercised. The corrected candidate paints intermediate timing pixels and completes springs.
Pan moves and returns on release (80,15); single tap increments to 1 and the
following double tap reports separately without increasing that count.
These are bounded device trials; busy-JS rendering and broader conformance
remain unqualified. See the [candidate receipt](validation/pr149-frame-mount-2026-11-13.md)
for hashes, negative controls and outstanding checks.

## Earlier device receipts (Pi 400, 2026-11-13)

Same build and probe as [Worklets](worklets-qnx.md#earlier-startup-probes-pi-400-2026-11-13).

- Reanimated: `makeMutable` with `withSpring(100)` and `withTiming(50)` ran on
  the UI thread; the completion callbacks reached JS with `finished=true` and
  the final values 100 and 50. A mid-animation read from JS returned an
  intermediate value (6.65 to 9.98 at 300 ms), so the UI runtime was advancing
  it.
- Not run on the device: `useAnimatedStyle` applied to a mounted item, the
  Advanced-tab cards, Gesture Handler pan and tap, Legend Motion and Legend
  List. The shared display was in use by other sessions, and injected pointer
  events did not reach this app's window, so no pointer-driven check was
  possible. Treat Gesture Handler and the cards as built and host-tested only.
