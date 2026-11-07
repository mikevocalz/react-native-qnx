# Reanimated and Gesture Handler: QNX platform integration map

Status: proposed mapping only. No library port, new runtime API, dependency change,
or device compatibility claim is introduced by this document.

The [November 7 exact-source audit](reanimated-gesture-handler-qnx-audit.md)
now records package targets, immutable source evidence, Expo version conflicts,
lean-Hermes delivery options and concrete QNX platform/bootstrap gaps. These
findings do not promote the libraries to supported QNX dependencies.

## Decision and ownership

Reanimated/Worklets and React Native Gesture Handler (RNGH) need platform
integration underneath application components. NYC-Tron consumes their familiar
React Native APIs; Nitro Forge owns document engines. Neither library should be
reimplemented separately inside each NYC-Tron component.

| Layer                                 | Responsibility                                                                                      | Location                                                                          |
| ------------------------------------- | --------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------- |
| React Native QNX host                 | Frame scheduling, event delivery, native view lookup, mounting, teardown                            | Existing `vnext/src/runtime`, `vnext/src/fabric`, `vnext/src/views/qt`            |
| Reanimated / Worklets QNX integration | Worklet runtime lifecycle, UI scheduling, animated prop updates, measurement and event hooks        | Version-pinned library platform port plus audited host hooks; exact files TBD     |
| RNGH QNX integration                  | Handler registry, recognition state, pointer data, gesture relationships and worklet event dispatch | Version-pinned library platform port plus existing Qt input seam; exact files TBD |
| NYC-Tron UI                           | Shared components and interactions consuming those APIs                                             | NYC-Tron repository; no QNX-specific gesture engine in each component             |
| Nitro Forge                           | Document operations, transactions, undo, serialization and editing computation                      | Forge Rust engines / Ignition                                                     |
| QNX vs MarathonOS                     | QNX QPA/Screen vs Linux/Wayland window and input integration                                        | Existing target-specific adapters; separate binaries and evidence                 |

The existing Qt Quick/Fabric renderer remains the only component backend.
Hermes stays the JavaScript engine; Yoga stays the layout authority. QML
properties and pointer handlers are useful comparisons, not substitutes for
upstream shared values, worklet semantics or RNGH's recognition state machine.

For example, a shared draggable layer control should consume RNGH/Reanimated.
Its QNX library ports update the existing Fabric-backed Qt item during the drag;
the application commits the resulting edit through Forge once the interaction
completes. Continuous visual preview must not create a second persisted document.

## Inspected baseline

Repository base: `e84b51430c239bc89dbd48ac35afab89f7c77c01`.

- `package.json` pins RN 0.88.1, React 19.3.0, Expo 58.0.22,
  Nitro/Nitrogen 1.0.0-alpha.nightly.105.g69d365fc9b18 and Metro 0.87.1.
  These are manifest observations, not registry-latest or ABI compatibility claims.
- Reanimated, Worklets and RNGH are not direct root dependencies in that manifest.
  Exact audit candidates are now recorded in the linked report; no QNX-compatible
  triplet is claimed by this mapping.
- `docs/component-support.md` reports partial transforms, native Animated,
  multitouch delivery and host evidence. Native Animated is not Reanimated.
- `QtTouchDispatcher.cpp` processes mouse and QTouchEvent contact sets, has
  responder targeting and hit testing, and dispatches RN touch events.
  Its header's single-pointer-only comment is stale relative to that implementation.
- That source filters TouchBegin/Update/End, but not TouchCancel; force is assigned
  1.0 and screenPoint is copied from pagePoint. These are specific audit items,
  not claims of complete input fidelity or an exhaustive source audit.
- Older handoff sections and README pins differ from the manifest and source.
  Preserve their historical evidence; rerun the chosen baseline rather than
  copying old green statuses into a new integration claim.
- No full RN + Reanimated + RNGH QNX device result was produced for this proposal.

Relevant existing contracts: [ADR-0001](adr/0001-qt-quick-backend.md),
[ADR-0002](adr/0002-qt-quick-item-instantiation.md),
[ADR-0009](adr/0009-fabric-only.md),
[ADR-0013](adr/0013-marathon-linux-wayland-native-target.md).

## Reanimated mapping

All rows describe planned integration responsibilities. Qt equivalents are
conceptual comparisons, not instructions to replace the upstream algorithm.

| React Native capability                  | QNX integration responsibility                                      | Qt relationship / limit                                                |
| ---------------------------------------- | ------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| Worklet runtime                          | Install and dispose the audited Worklets runtime and host bindings  | QML JavaScript is not the worklet runtime                              |
| Shared values                            | Preserve upstream synchronization and ownership across runtimes     | Q_PROPERTY is a visual sink or conceptual analogue only                |
| Derived values / reactions               | Preserve upstream dependency tracking and execution ordering        | Do not rebuild a parallel QML dependency graph                         |
| Animated styles / props                  | Resolve live Fabric targets and apply supported properties safely   | Update existing Qt items through an audited mounting/property seam     |
| Timing / spring / decay                  | Execute selected upstream algorithms against the host frame clock   | QML NumberAnimation/SpringAnimation/Flickable are not numerical parity |
| Sequence / repeat / delay / cancellation | Preserve upstream sequencing, completion and interruption semantics | No separate Qt timer for every animated property                       |
| Frame callbacks                          | Connect a monotonic frame source to the selected runtime            | Audit QQuickWindow/render-loop integration; do not assume fixed 60 Hz  |
| UI-to-JS and JS-to-UI scheduling         | Use audited runtime dispatch with ordering and teardown guarantees  | Never execute JSI on an arbitrary Qt render thread                     |
| Animated events                          | Route normalized native events to worklet handlers                  | Coalescing must preserve terminal and cancellation events              |
| measure / scrollTo                       | Resolve mounted tags, coordinate spaces and existing scroll bridge  | Transforms, scale, nested scroll and unmount must be accounted for     |
| Layout / entering / exiting              | Integrate upstream Fabric hooks and view lifetime                   | Yoga remains authoritative; Qt anchors/layouts cannot compete          |
| Reduced motion                           | Expose actual platform preference or documented app policy          | Do not invent a QNX OS setting                                         |
| Reload / unmount / background            | Cancel outstanding work and invalidate view/runtime references      | No callbacks after runtime or QObject destruction                      |
| Unsupported properties                   | Report a precise capability or failure                              | No silent success for perspective/skew/matrices not yet supported      |

Qt Animator types can run on a rendering thread under a threaded render loop,
but their QML properties are not continuously updated during playback. They are
not a general worklet implementation. Any optimized scene-graph path needs
explicit synchronization with the authoritative animated value and hit testing.
QQuickItem/QObject mutations must execute on their owning GUI thread. Worklet
scheduling never authorizes direct access from a worklet or render thread.

## Gesture Handler mapping

| Gesture capability                    | Qt input comparison                                    | Required RNGH/platform behavior                                            |
| ------------------------------------- | ------------------------------------------------------ | -------------------------------------------------------------------------- |
| Tap / multi-tap                       | TapHandler                                             | Match thresholds, timing, button filters and recognition states            |
| Long press                            | TapHandler long press                                  | Match duration, movement limits and cancellation                           |
| Pan                                   | DragHandler                                            | Match activation/failure bounds, pointer counts, translation and velocity  |
| Pinch                                 | PinchHandler                                           | Stable identities, focal point, scale and contact transitions              |
| Rotation                              | PinchHandler rotation                                  | Preserve angle units, accumulation and pointer changes                     |
| Fling                                 | Native event velocity; Flickable is scroll behavior    | Implement selected RNGH direction/velocity criteria                        |
| Hover                                 | HoverHandler                                           | Device and pointer type filters, enter/move/exit                           |
| Manual gesture / raw touch            | QPointerEvent / QTouchEvent / PointHandler             | Explicit handler state transitions and normalized payloads                 |
| Native gesture / scroll coordination  | Existing Flickable / scroll bridge                     | Coordinate native scrolling, responder claims and handler activation       |
| Simultaneous / exclusive / race       | Qt passive/exclusive grabs are only related mechanisms | Preserve upstream composition semantics; grabs alone are insufficient      |
| Require failure / block relationships | No direct one-to-one Qt primitive                      | Validate handler dependency graph, ordering and cancellation               |
| Stylus data                           | Qt tablet/pointer data where delivered                 | Preserve actual pressure/tilt/buttons; report unavailable values honestly  |
| Wheel / trackpad                      | WheelHandler / Qt wheel events                         | Map supported library input behavior; distinguish scroll from touch pan    |
| Handler roots / nested surfaces       | Existing Fabric surface and tag registry               | Scope identities, modal roots and target ownership                         |
| Touch cancellation                    | Qt cancel/grab loss/window deactivation                | Deliver correct cancellation on OS interruption, unmount and root teardown |
| Hit slop / transformed coordinates    | Qt item geometry and mapping                           | Match upstream hit-testing rules, clipping and coordinate units            |
| Touch/mouse synthesis                 | Qt event source information                            | Prevent duplicated gestures from synthesized mouse events                  |

Prefer one normalized pointer stream feeding RN responders and RNGH through
explicit arbitration. Do not run independent QML recognizers and RNGH recognizers
over the same stream without a single ownership/cancellation policy.

## Existing source seams and future ownership

These are locations to inspect/extend, not declarations that their public APIs
already support the required integrations.

| Workstream                      | Existing path                                                                | Deliverable before runtime changes                                 |
| ------------------------------- | ---------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| Platform architect / integrator | This document; `docs/adr`                                                    | Selected version matrix and ownership decision                     |
| Hermes / JSI engineer           | `vnext/src/runtime/QtReactHost.cpp` and corresponding header                 | Scheduler/runtime lifetime source audit                            |
| Qt Quick / Fabric engineer      | `vnext/src/fabric/QtQuickMountingLayer.cpp`, `ShadowViewMutationAdapter.cpp` | Animated prop, measurement and mount-conflict design               |
| Input integration engineer      | `vnext/src/fabric/QtTouchDispatcher.cpp`, `QtScrollBridge.cpp`               | Contact identity, cancellation, scroll and recognition arbitration |
| Metro / dependency engineer     | `packages/metro-config-qnx`, `package.json`, `pnpm-lock.yaml`                | Matching Reanimated/Worklets/RNGH pins and QNX resolution proof    |
| Build engineer                  | `cmake/RnqnxReactQt.cmake`, `CMakeLists.txt`                                 | Separate opt-in library integration targets                        |
| QNX regression owner            | `native/platform/qnx`, Pi/runtime scripts                                    | Existing QNX evidence preserved and new target results labeled     |
| Linux / Wayland engineer        | `native/platform/linux-host`, Marathon target                                | Independent lifecycle, input and frame evidence                    |
| QA reviewer                     | `tests`, `apps/rn-playground`                                                | Behavioral conformance and shutdown/stress cases                   |

Host platform hooks belong here. Upstream-specific changes may live in reviewed
forks or patches of the libraries until accepted upstream. Preserve library import
names where feasible, but do not promise unchanged imports until Metro, codegen,
native registration and runtime tests prove them. This does not authorize a new
application-facing Nitro API or replacement legacy bridge.

## Implementation sequence for later approval

1. Resolve the exact-source audit's version/Expo and bootstrap-policy decisions,
   then pin the integration graph. Preserve its source commits, native interfaces,
   Babel/bytecode requirements, platform exclusions and license record.
2. Add opt-in build/resolver seams with explicit unsupported errors and default-OFF
   experimental switches. Preserve the existing QNX bundle when disabled.
3. Prove one opacity/translation timing animation, interruption and unmount through
   the real upstream runtime on the Qt host. Then cross-build and execute on QNX.
4. Prove a pan gesture driving a shared value and the same Qt-backed Fabric view;
   add cancellation and nested ScrollView arbitration before broader gestures.
5. Expand to multitouch, composition, stylus, measurement and layout animation.
   Gate each capability separately rather than publishing a global supported flag.
6. Exercise a NYC-Tron control and one Forge edit transaction through the same
   platform integrations. Keep document undo independent of animation frames.

## Acceptance evidence required

| Gate                | Required evidence                                                                                              |
| ------------------- | -------------------------------------------------------------------------------------------------------------- |
| Dependency / source | Exact package pins, lockfile, peer compatibility, upstream symbols and license audit                           |
| Metro / Babel       | QNX production bundle with actual worklets transformation; disabled-path regression comparison                 |
| Host runtime        | Real worklet execution, gesture recognition, visible prop changes and measured ordering                        |
| Ownership           | Runtime reload, unmount during gesture/animation, destroyed target, no callbacks after teardown                |
| Behavior            | Spring/decay interruption; gesture composition; cancellation; transformed hit testing; nested scrolling        |
| QNX                 | Licensed-toolchain cross-build plus actual RN/Qt/Reanimated/RNGH execution; Screen probe alone is insufficient |
| Marathon            | Separate Linux/Wayland input/frame/lifecycle evidence; no QNX binaries or official store claim                 |
| Performance         | Frame pacing, dropped frames, input-to-presentation latency, CPU and memory on named hardware                  |
| Forge consumer      | Drag preview followed by one engine transaction; cancel leaves document unchanged                              |

At 60 Hz the total frame interval is about 16.7 ms; at 120 Hz it is about 8.3 ms.
These are frame-budget references, not measured results or library guarantees.
Keep per-event heavy work out of the JS and Qt GUI threads.

## Validation and rollback for this mapping

This change only adds documentation/evidence. The audit includes isolated tooling
installation and a Babel/HBC smoke check. No application dependency installation,
native build, full Metro bundle, sanitizer run or device test is claimed for it.
Source paths and links are checked against the inspected repository tree; an
independent architecture review checks the proposal. Runtime gates above remain
requirements for a later implementation PR, not blockers to reviewing this map.

Rollback is deletion/revert of this document. There is no runtime switch or package
change to undo. Existing Qt/QNX licensing and Marathon permission gates remain
unchanged; no Qt source is downloaded or distributed by this PR.

## Sources

- [Qt input handlers](https://doc.qt.io/qt-6/qtquickhandlers-index.html)
- [Qt pointer handler ownership](https://doc.qt.io/qt-6/qml-qtquick-pointerhandler.html)
- [Qt animations](https://doc.qt.io/qt-6/qtquick-statesanimations-animations.html)
- [Qt Animator thread/property semantics](https://doc.qt.io/qt-6/qml-qtquick-animator.html)
- [Reanimated documentation](https://docs.swmansion.com/react-native-reanimated/)
- [Worklets documentation](https://docs.swmansion.com/react-native-worklets/)
- [Gesture Handler documentation](https://docs.swmansion.com/react-native-gesture-handler/)

Qt documentation is a capability reference; availability must be checked against
the eventual pinned SDK. Upstream library source/API compatibility audit is
explicitly deferred until an implementation version set is selected.
