# ADR-0021: JavaScript on its own thread, off the Qt GUI thread

## Status

Proposed, 2026-11-21. Behind `QtReactHost::Options::jsThread`, default off.
Host evidence only (see Results). No QNX VM, Pi 400 or release-board run
exists, so the `window-chrome` and `device-parity` gates stay `pending`.
Time box: one working session. Owner decision 3 applies if the device run
cannot be made.

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

| Thread | Owns | Never does |
| --- | --- | --- |
| GUI (constructing) thread | `QQuickWindow`, every `QQuickItem`, `QtQuickMountingLayer`, `ShadowViewMutationAdapter`, `QtTouchDispatcher`, `QtSchedulerDelegate` mount calls, the title bar chrome, `QScreen` reads, the UI-worklet runtime when Worklets is enabled (ADR-0020) | Touch Hermes, JSI values or the `RuntimeScheduler` |
| JS thread `rnqnx-js` (`QtJsThread`, a `QThread` with an event loop and an 8 MiB stack) | Hermes, `RuntimeScheduler`, `Scheduler` and `UIManager`, JS timers (`QTimer`), the animation `QTimer`, `QtRuntimeTaskQueue` and its wakeup `QObject`, `QtEventBeat`, TurboModule instances, `QtReactHost::Impl` | Touch a Qt Quick item |
| Qt thread pool, module-owned threads | Image I/O and decode, Nitro engines | Touch JSI values; they complete through `QtRuntimeTaskQueue::post` |

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
creates there, with `Qt::QueuedConnection`. The GUI thread pulls the
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
changed, and no Nitro test ran in this mode (see Not verified).

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
AppState module needs the GUI thread for its constructor and signals, so a
lookup of it in this mode throws. All four stay on the single-thread path until
each is moved and tested. The Nitro runtime installers, `QnxDevSupport` and the
WebSocket, FileReader and image modules were not run in this mode.

Two shared pieces needed a change because both threads now touch them.
`ImageLoadedNotifier` is process-wide and was unlocked; it now takes a mutex and
calls listeners outside it. `QnxDeviceInfo` read `QScreen` on the lookup thread;
the host reads the screen on the GUI thread at construction and hands it over.

Known and left: resize and window-size posts are not coalesced, so a long drag
during a long block queues one call per event and replays them when JavaScript
is free. `QtJsThread::post` and `call` check `isRunning()` and then enqueue; a
`call` racing `quit()` could wait forever, which only the destructor can cause.

## Results

Filled in after the runs below. Every row names its target.

| Check | Target | Result |
| --- | --- | --- |
| `qt_react_host_js_thread_host_test`, 6 cases, ASan, title bar ON | host: macOS 26 arm64, Qt 6.12.0, Hermes Debug, offscreen | see PR |
| JS blocked 5 s: pan tracks, minimize and close act, resize calls return, heartbeat runs | host, same build | see PR |
| ThreadSanitizer | host | see PR |
| Same checks on QNX VM, Pi 400 and the release board | not run | The Pi and the T9 drive were not available for a device run |

## Consequences

- With the flag on, a script that spins for five seconds no longer freezes the
  window controls or native pan. This is the property the `window-chrome` gate
  line needs, shown on the host only.
- Start-up code that calls awaited methods from an input handler would still
  block. The header lists which calls those are.
- Libraries that want the runtime from the GUI thread must move to
  `runOnJsThread` before they can run in this mode. Three refuse to install.
- A second thread adds a hop to every mount: JS to GUI through a queued call.
  One extra event-loop turn per transaction. The frame-time cost has not been
  measured, on any target.

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
- `vnext/src/runtime/QtRuntimeTaskQueue.cpp`
- `tests/cpp/QtReactHostJsThreadHostTest.cpp`
- `docs/adr/0016-window-chrome-on-qnx-screen.md`, `docs/adr/0020-worklets-reanimated-gesture-handler-bootstrap.md`
- `docs-site/content/docs/explanation/threading.mdx`

## Notes

- Device run still owed: 20 cold boots (S9), a blocked-JS run on the Pi 400,
  and the frame-time cost of the extra hop against the S4 budgets. Nothing in
  this ADR loosens a budget or a gate.
- Open: moving Worklets, Reanimated, Gesture Handler, Nitro installers and the
  dev client to `runOnJsThread`, then removing the three install guards.
