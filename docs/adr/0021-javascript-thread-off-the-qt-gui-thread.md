# ADR-0021: JavaScript on its own thread, off the Qt GUI thread

## Status

Proposed, 2026-11-21. Behind `QtReactHost::Options::jsThread`, default off.
Evidence is from the host and from the QNX VM (QEMU, TCG emulation, software
render; not the Pi 400, not the release board). The VM run is in
[docs/evidence/s2-vm-2026-11-21](../evidence/s2-vm-2026-11-21/README.md). No Pi
400 or release-board run exists, so the `window-chrome` and `device-parity`
gates stay `pending`. Time box: one working session. Owner decision 3 applies
if the device run cannot be made.

## Context

`QtReactHost` runs Hermes, the React Native `RuntimeScheduler`, the Fabric
mount and every Qt Quick item on the thread that constructs it, the
`QGuiApplication` thread. ADR-0016 records the consequence: while JavaScript
is busy, the title bar close, minimize and maximize controls do not respond,
because they are Qt Quick items on the same thread. The same holds for any
native gesture, since pointer events wait in the same event loop.
`docs-site/content/docs/explanation/threading.mdx` has the full map of the
single-thread arrangement.

The v1 gate `window-chrome` asks for chrome that works while JS is blocked.
The v1 gate `device-parity` asks the same of a pan gesture.

## Decision

JavaScript moves to a thread the host owns, when `Options::jsThread` is set.
The constructing thread stays the GUI thread. With the flag off, nothing
changes: one thread, same code path, same tests.

### Thread map (flag on)

| Thread                                                                                 | Owns                                                                                                                                                                                                                                               | Never does                                                         |
| -------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| GUI (constructing) thread                                                              | `QQuickWindow`, every `QQuickItem`, `QtQuickMountingLayer`, `ShadowViewMutationAdapter`, `QtTouchDispatcher`, `QtSchedulerDelegate` mount calls, the title bar chrome, `QScreen` reads, the UI-worklet runtime when Worklets is enabled (ADR-0020) | Touch Hermes, JSI values or the `RuntimeScheduler`                 |
| JS thread `rnqnx-js` (`QtJsThread`, a `QThread` with an event loop and an 8 MiB stack) | Hermes, `RuntimeScheduler`, `Scheduler` and `UIManager`, JS timers (`QTimer`), the animation `QTimer`, `QtRuntimeTaskQueue` and its wakeup `QObject`, `QtEventBeat`, TurboModule instances, `QtReactHost::Impl`                                    | Touch a Qt Quick item                                              |
| Qt thread pool, module-owned threads                                                   | Image I/O and decode, Nitro engines                                                                                                                                                                                                                | Touch JSI values; they complete through `QtRuntimeTaskQueue::post` |

`QtReactHost::Impl` is constructed, used and destroyed on the JS thread. The
constructor of `QtRuntimeTaskQueue` records that thread as its owner, so
`runSync`, `enterJS` and `close` require the JS thread, as before.

### Who owns Hermes

`QtJsThread` owns the thread. `Impl` owns the runtime and is the only code that
touches it. `QtReactHost::runtime()` hands out the reference only on the JS
thread; any other caller prints `RNQNX_FATAL` and terminates, which is the rule
`QtSchedulerDelegate::onOwnerOrPost` already applies. Code that needs the
runtime from the GUI thread uses `runOnJsThread`.

The `runtimeExecutor` given to the `RuntimeScheduler` runs the callback inline
when already on the JS thread, so `evaluate` keeps its synchronous contract.
From any other thread it posts the callback through a gate that holds a mutex
and an open flag. `~QtReactHost` closes the gate on the JS thread after
`shutdown()`, so a worker that still holds a `CallInvoker` finds a closed gate
and drops the callback, and a callback queued earlier checks the flag again on
the JS thread before it touches `Impl`. The UIManager's
executor goes through `scheduleWork`, so AppRegistry start and stop cross the
same way.

### Public API: awaited, posted, thread-bound

- Awaited, for start-up and teardown: `evaluate`, `startSurface`,
  `startEmptySurface`, `stopSurface`, `commit`, the `install*` calls,
  `registerTurboModule`, `addTeardownHook`, `setTurboModuleProvider`,
  `hasFabricUIManager`, `lastError`, `javaScriptErrors`, `pumpEvents`. The
  caller blocks until the JS thread finishes, then applies the mounts that
  call had already queued (`flushMounts`). Mounts that arrive later, from a
  timer, a microtask or the event beat, go through the GUI event loop as they
  do for any other queued call. These calls block the GUI thread for as long
  as JavaScript stays busy, so an input handler must not call them.
  `javaScriptErrors` returns a snapshot that the next call replaces; two
  threads must not call it at once. Teardown hooks and TurboModule factories
  run on the JS thread and must not wait on the GUI thread, which the
  destructor holds.
- Posted, for GUI handlers: `resizeSurface`, `setWindowSize` (the `QScreen`
  read happens on the caller), `callFunctionOnModule`.
- Thread-bound: `runtime()`.
- Rejected: `setOwnerPoster` throws in this mode, since the host's own poster is
  what moves mounts to the GUI thread.

### How mount transactions reach the GUI thread

`schedulerShouldRenderTransactions` fires on the JS thread, inside the
rendering update of a scheduler task. `QtSchedulerDelegate` is constructed with
the GUI thread id as its owner. `onOwnerOrPost` finds the caller is not the
owner and posts the same call to the GUI thread through a `QObject` the host
creates there (`QueuedCallTarget::postCall`). The GUI thread pulls the
transaction from the `MountingCoordinator`, applies it to the adapter and
reports the mount. This is the path the UI-worklet thread already uses
(`setOwnerPoster`); the host now installs it by default in this mode.
`schedulerDidDispatchCommand`, `schedulerShouldSynchronouslyUpdateViewOnUIThread`
and `schedulerDidSetIsJSResponder` take the same route. The last one was
missing it, since the touch dispatcher belongs to the GUI thread.

GUI to JS: `QtTouchDispatcher` already hands events to Fabric event emitters,
whose queue reaches the JS thread through the `runtimeExecutor`. No change.

### Nitro dispatcher

`QtNitroDispatcher` wraps `QtRuntimeTaskQueue`. Because the queue's owner is the
JS thread in this mode, `isOnDispatcher` is true on the JS thread, `runSync`
requires it, and `runAsync` from a worker lands there. No dispatcher code
changed. The Nitro installer and the dispatcher ran in this mode on the QNX VM
(see Results); the Nitro hybrid objects themselves did not.

### Shutdown order

`~QtReactHost`, on the GUI thread:

1. On the JS thread: `Impl::shutdown()`. Stop and unregister surfaces, run
   teardown hooks in reverse order, invalidate AppState, close the task queue
   (drains accepted work).
2. On the GUI thread: apply the mounts step 1 queued.
3. On the JS thread: destroy `Impl`.
4. Join and delete the JS thread, then delete the GUI context object, which
   drops any mount still queued.

Mounts queued from step 1 capture the delegate, so they must run before step 3.

### Not moved in this change (fails closed)

`QtWorkletsPlatform::install`, `QtGesturePlatform::install` and
`installReanimated` read the runtime from the GUI thread and run worklets against
it. In JS-thread mode they throw `std::runtime_error` naming this ADR. The
AppState module needed the GUI thread for its constructor and signals, so a
lookup of it in this mode threw; [ADR-0024](0024-minimize-on-qnx-screen.md)
moves its observer to the GUI thread and gives JavaScript an atomic cache, so
AppState now loads in this mode (host-compiled only; the QNX VM result above
predates it). The other three stay on the single-thread path until
each is moved and tested. On the QNX VM all four refused with their
ADR-0021 message (see Results). `QnxDevSupport` was not run in this mode. The
Nitro runtime installer, the WebSocket and FileReader modules and
`ImageLoader.getSize` ran in this mode on the QNX VM; the Expo image view and
the image modules beyond `getSize` did not.

Two shared pieces needed a change because both threads now touch them.
`ImageLoadedNotifier` is process-wide and was unlocked; it now takes a mutex and
calls listeners outside it. `QnxDeviceInfo` read `QScreen` on the lookup thread;
the host reads the screen on the GUI thread at construction and hands it over.

Known and left: resize and window-size posts are not coalesced, so a long drag
during a long block queues one call per event and replays them when JavaScript
is free. `QtJsThread::post` and `call` check `isRunning()` and then enqueue; a
`call` racing `quit()` could wait forever, which only the destructor can cause.

## Results

Every row names its target. All host runs: macOS arm64, Apple clang, Qt 6.12.0
(Homebrew), Hermes built from the pinned source in Debug, offscreen platform,
software Qt Quick, window chrome ON.

| Check                                                                                             | Target                                    | Result                                                                                                                                                  |
| ------------------------------------------------------------------------------------------------- | ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `qt_react_host_js_thread_host_test`, 6 cases, ASan                                                | host                                      | 6/6 pass. In the blocked case: block 5003 ms, 234 heartbeats of 10 ms by 2500 ms, 484 in all                                                            |
| Same case with the flag off (mutation: `Fixture(false)`)                                          | host                                      | fails at `!blockDone`, as it must                                                                                                                       |
| `qt_react_host_host_test`, flag off                                                               | host                                      | 62/62 pass                                                                                                                                              |
| Same JS-thread test, whole graph built with `-fsanitize=thread`                                   | host                                      | exit 0, 0 ThreadSanitizer reports                                                                                                                       |
| Negative control: the acquire in `QueuedCallTarget::event` removed                                | host                                      | 42 reports, so the run can see the hand-off                                                                                                             |
| Linux, Qt 6.11.1 (CI): both renderer legs run the test                                            | Linux CI                                  | see PR #201                                                                                                                                             |
| ThreadSanitizer workflow on main, first Linux run (run 44919044567)                               | Linux CI                                  | red: 21 reports, all a Qt allocation freed on Qt's QML loader thread; see the ThreadSanitizer note                                                      |
| Same workflow with `ignore_noninstrumented_modules=1`, branch commit `6d42c522` (run 44921312130) | Linux CI                                  | green: 9 cases pass, 0 ThreadSanitizer reports                                                                                                          |
| QNX VM, 3 runs of the 11-case cross-build, JS blocked 5 s, flag on                                | QNX VM (TCG, software render, not the Pi) | 3/3 pass. Pan moved the Flickable (contentY 0 to 48), minimize 11 to 12 ms, close 5 to 6 ms, resize calls 0 ms, 196 to 198 heartbeats of 10 ms by 2.5 s |
| Same block with the flag off (control)                                                            | QNX VM (TCG, software render, not the Pi) | 0 heartbeats during the block, pan unmoved at block end, a queued close click acted 5003 to 5004 ms in, after the block                                 |
| Touch events during the block, flag on                                                            | QNX VM and host                           | 10 dispatched natively; JavaScript saw 2 starts, 2 ends and 2 moves after the block (Fabric coalesces queued moves per view)                            |
| Worklets, Reanimated, Gesture Handler, AppState in this mode                                      | QNX VM                                    | each refused with its ADR-0021 message                                                                                                                  |
| Nitro installer and dispatcher, WebSocket loopback, FileReader, `ImageLoader.getSize`             | QNX VM                                    | pass in this mode; the WebSocket events reached JavaScript on the JS thread                                                                             |
| Pi 400, release board                                                                             | not run                                   | The Pi and the T9 drive were not available for a device run                                                                                             |

Scope of the ThreadSanitizer result: Qt, the C++ runtime and system libraries
are not instrumented. Qt's own queue cannot be seen, so every hand-off the host
makes goes through `QueuedCallTarget`, which carries its work in a `QEvent` the
host allocates and marks release and acquire under ThreadSanitizer. Before that
change the same run reported 93 races, all of them the callable crossing Qt's
queue. Anything that reaches the JS thread by another route would be reported
or, if it only touches uninstrumented code, missed.

ThreadSanitizer note, 2026-11-21. The first Linux run of the workflow (on main
`5b691720`, run 44919044567) failed with 21 reports. Each pairs an allocation
on the GUI thread with its free on Qt's `QQmlThread`, with every frame in Qt
(`libQt6Core`, `libQt6Qml`) except the test body: Qt hands the memory over
through its own queue, which an uninstrumented Qt hides. The macOS run in the
first receipt had none because macOS defaults `ignore_noninstrumented_modules`
to on; Linux defaults it to off. The workflow now sets it, so allocation and
synchronization calls made from Qt are not tracked and the host's own code and
its `QueuedCallTarget` hand-offs still are. The macOS negative control (42
reports with the acquire removed) ran under that same default. A Linux negative
control was not run.

## Consequences

- With the flag on, a script that spins for five seconds no longer freezes the
  window controls or native pan. This is the property the `window-chrome` gate
  line needs, shown on the host and on the QNX VM (synthetic pointer events,
  software render, TCG), not on a Pi 400 or the release board.
- Start-up code that calls awaited methods from an input handler would still
  block. The header lists which calls those are.
- Libraries that want the runtime from the GUI thread must move to
  `runOnJsThread` before they can run in this mode. Three refuse to install.
- A second thread adds a hop to every mount: JS to GUI through a queued call.
  One extra event-loop turn per transaction. The frame-time cost has not been
  measured, on any target. The VM timings are TCG emulation and are not
  performance evidence.

## Alternatives considered

- Leave JS on the GUI thread and document the limit. This is owner decision 3's
  fallback. It remains the position if the device run cannot be made.
- Run only Hermes on a thread and keep the `RuntimeScheduler` on the GUI thread.
  The scheduler's rendering update decides when to mount; splitting it from the
  runtime would need a lock around every task.
- Post every call, never wait. `evaluate` returns a success value that callers
  and tests read at once, and start-up order depends on it.
- Instrument with ThreadSanitizer only. See Notes for why that alone is not
  enough.

## Related files

- `vnext/include/react-native-qnx/runtime/QtJsThread.h`, `vnext/src/runtime/QtJsThread.cpp`
- `vnext/include/react-native-qnx/runtime/QtReactHost.h`, `vnext/src/runtime/QtReactHost.cpp`
- `vnext/include/react-native-qnx/runtime/QueuedCallTarget.h`, `vnext/src/runtime/QueuedCallTarget.cpp`
- `vnext/src/runtime/QtRuntimeTaskQueue.cpp`
- `.github/workflows/js-thread-tsan.yml` (manual dispatch)
- `tests/cpp/QtReactHostJsThreadHostTest.cpp`
- `docs/adr/0016-window-chrome-on-qnx-screen.md`, `docs/adr/0020-worklets-reanimated-gesture-handler-bootstrap.md`
- `docs-site/content/docs/explanation/threading.mdx`

## Notes

- Device run still owed: 20 cold boots (S9), a blocked-JS run on the Pi 400,
  and the frame-time cost of the extra hop against the S4 budgets. Nothing in
  this ADR loosens a budget or a gate.
- The playground and consumer `main.cpp` construct `QtReactHost` without the
  option and read `host.runtime()` on the GUI thread, so no app binary runs in
  this mode yet. The VM run used the cross-built test, not the showcase app.
  Real Screen pointer input was not used: the VM has no input device, so the
  press, drag and release are `QMouseEvent`s sent to the window.
- Open: moving Worklets, Reanimated, Gesture Handler and the dev client to
  `runOnJsThread`, then removing the three install guards and the AppState
  guard. The Nitro installer needs no change.
