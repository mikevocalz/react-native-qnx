# Worklets on QNX

Stage 2 of the [v2 plan](v2/README.md). `react-native-worklets` 0.13.1 (the
version main already resolves, inside the 0.13.x line that supports React
Native 0.86 to 0.88) runs its UI Worklet Runtime on a dedicated thread of the Qt
host. The upstream C++ is compiled from the installed npm package; nothing from
it is vendored. The bootstrap exception is recorded in
[ADR-0020](adr/0020-worklets-reanimated-gesture-handler-bootstrap.md).

## Enable

```sh
cmake ... -DRNQNX_ENABLE_WORKLETS=ON \
  -DRNQNX_WORKLETS_SOURCE_DIR=<app>/node_modules/react-native-worklets
```

The switch defaults to OFF and the CMake step rejects any version outside
0.13.x. `babel-preset-expo` adds the `react-native-worklets/plugin` Babel plugin
by itself when the package is installed, so an app needs no Babel change.

## Threads

| Thread                        | Runs                                                                                          |
| ----------------------------- | --------------------------------------------------------------------------------------------- |
| Qt GUI thread = RN JS thread  | React, Hermes RN runtime, module calls, Qt item changes, `afterAnimating` signal              |
| `rnqnx-ui-worklet` (new)      | UI Worklet Runtime, every worklet passed to `scheduleOnUI`, `requestAnimationFrame` callbacks |
| Qt render thread (if present) | Scene graph rendering only; no worklet or React code runs there                               |

The host still runs React Native on the thread that owns Qt, so the UI-worklet
thread is the third thread, not a replacement for the JS thread. The seams
between them:

- UI to JS: `QtTaskQueueCallInvoker` posts to the host's `QtRuntimeTaskQueue`.
  React Native's own call invoker cannot be used here, because this host's
  runtime executor runs the callback inline on the caller and would execute
  JavaScript on the UI-worklet thread.
- JS to UI: `QtUiThread::post`, one queued call per batch (`QtUIScheduler`).
- Frames: `QtFrameBridge` marks the window dirty on a request and delivers one
  batch per `QQuickWindow::afterAnimating` on the GUI thread, then runs the
  callbacks on the UI thread with a `steady_clock` millisecond timestamp. No
  timer is involved. A window that is hidden or not exposed produces no frames,
  and therefore no callbacks. One window is followed.
- Qt items are only changed on the GUI thread. A commit made from the UI thread
  (Reanimated does this) mounts through `QtSchedulerDelegate`'s owner poster.

## Teardown

`QtReactHost::addTeardownHook` runs the platform's shutdown while Hermes is
alive: stop frames, mark the RN runtime dead, invalidate the proxy, join the UI
thread. Jobs queued but not started are discarded. The UI runtime object itself
lives until the RN runtime releases its handle during host destruction.

## Patches to upstream

One line of `AsyncQueueImpl.cpp` is replaced in a generated copy
(`pthread_setname_np(name)` is the Apple form; QNX takes the thread first).
CMake fails the configure step if the line changes. Nothing else is modified.

## Device receipts (Pi 400, 2026-11-13)

Build: QNX SDP 8.0.5, full Hermes, `-O1`, Worklets 0.13.0 (the device runs predate the bump to 0.13.1; the C++ is the same
line and has not been rerun). A startup probe in a
local copy of the gallery (not committed) logged from the app's stderr:

- A worklet passed to `scheduleOnUI` reported its thread as `ui-worklet`; the
  JS runtime reported `gui`.
- A 20-callback `requestAnimationFrame` chain ran from Qt Quick
  `afterAnimating`: 20 frames in 480 ms with the window visible (about 41 fps,
  not tuned), 61 of 63 signalled frames delivered.
- Teardown: a never-ending frame loop, 500 queued UI jobs and 2000 queued
  UI-to-JS posts, then exit through `--screenshot`. Five of five runs exited
  with status 0 and logged `RNQNX_WORKLETS_SHUTDOWN_OK`.
- A worklet that throws (calling `scheduleOnRN` with a locally defined function)
  reaches the host's fatal-error path and terminates the process, as a fatal
  JS error does.

Not measured: frame pacing against the display, behavior while JS is busy.

## Limits

- Bundle Mode is not supported and reports so when used.
- Source unpackers need a Hermes that compiles source. The lean Hermes used for
  release builds rejects source, so a release build needs the bytecode unpacker
  path (Babel plugin `hermesBytecode: true`) proved on the target; this change
  proves the source-unpacker path on full Hermes only.
- `toggleSlowAnimationsOnUIRuntime` is a no-op that returns false.
- JavaScript load blocks the GUI thread on this host, so while JS is busy the
  screen cannot repaint and frame callbacks do not fire. Work already queued on
  the UI thread still runs. See [Reanimated and Gesture Handler](reanimated-gesture-handler-qnx.md).
- Networking inside worklet runtimes (Bundle Mode only) is not provided.
