# ADR-0024: Minimize on QNX Screen, and what AppState reports

## Status

Proposed, 2026-11-23. The implementation and its host tests exist (macOS host, Qt 6.12.0). Nothing here has run on the QNX VM or the
Pi 400, and AppState in JavaScript-thread mode has been compiled on the host only. The status moves to Accepted when the QNX VM
and the Pi 400 receipts named under Notes pass. The `window-chrome` gate rule "an ADR in docs/adr defines minimize on QNX Screen"
is met by this text once it is Accepted.

## Context

[ADR-0016](0016-window-chrome-on-qnx-screen.md) item 3 made minimize destroy the app's native window under the stock
`fullscreen-winmgr`, because that manager shows one window per display, sizes it to the display, and has no message that raises
the launcher. The process, the `QQuickWindow`, its items and the React tree stay. ADR-0016 left `AppState` unchanged: Qt does not
reliably change `QGuiApplication::applicationState()` when a platform window is destroyed, so JavaScript saw `active` while the
window was gone. The `window-chrome` gate asks for "minimize and restore with the AppState transition logged".

Two more facts shape the decision. `rnqnx-winmgr` (tools/qnx-winmgr) keeps app windows at their own size, lists them in a dock,
and takes `rnqnx=minimize` and `rnqnx=restore` manager messages. [ADR-0021](0021-javascript-thread-off-the-qt-gui-thread.md)
puts JavaScript on its own thread behind `QtReactHost::Options::jsThread`, and the AppState module refused to load there.

## Decision

### What minimize does

| Window manager       | Minimize                                                                                                                                                                              | Restore                                                                                                                                                                                                  |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `fullscreen-winmgr`  | `minimizeByDestroyingWindow`: `QWindow::hide()` then `QWindow::destroy()`. The launcher shows because no app window is left. The window is not in the manager's Alt+Tab list.          | A second launch of the app (launcher tile, run script, or `--restore-only`) sends `restore` over the one-instance socket. The host shows the window again, recreating it when it sits behind another one. |
| `rnqnx-winmgr`       | `sendWindowManagerRequest(window, "minimize")`: the manager message `rnqnx=minimize`. The manager hides the window and lists it in its dock. The native window and process stay.      | `rnqnx=restore`, from the dock or from a second launch. The manager shows and raises the window.                                                                                                          |
| Off QNX (host tests) | `showMinimized()`. AppState does not follow it; this ADR defines QNX behaviour only.                                                                                                      | `restoreMinimizedWindow`.                                                                                                                                                                                |

### What JavaScript sees

The host decides, not Qt. `rnqnx::runtime::HostWindowState` (native/lifecycle) holds one atomic `minimized` flag. The code that
minimizes sets it before the window is hidden: `minimizeByDestroyingWindow` and a `minimize` manager request set it, and
`restoreMinimizedWindow` and a `restore` request clear it. `QtApplicationLifecycle` follows it:

| Event                                                    | `AppState.currentState` and the `change` event |
| -------------------------------------------------------- | ---------------------------------------------- |
| Host minimizes (either manager)                          | `background`                                   |
| Qt reports inactive, hidden or suspended while minimized | no change, still `background`                  |
| Host restores                                            | `active`, then whatever Qt reports next        |
| Qt reports active while the host still says minimized    | the host flag clears and JavaScript sees `active`; this is the window manager restoring the window without telling the host |
| Minimize or restore requested twice                      | one transition, not two                        |

`Hidden` and `Suspended` map to `background`, as before. On a Qt state change that does not involve minimize, behaviour is
unchanged. For the windows API the event order on minimize is: visibility false, then focus false. On restore it is the reverse.

### AppState with JavaScript on its own thread

The Qt observer is created, read and destroyed on the GUI thread only. JavaScript reads an atomic cache and gets changes through
the existing runtime-scheduler call invoker, whose `invokeAsync` is thread safe. In `jsThread` mode the module is built on the
JavaScript thread, so:

- Its first value comes from atomics (`HostWindowState::minimized()` and a mirror of `applicationState()` attached on the GUI
  thread in the `QtReactHost` constructor), never from a Qt object.
- The observer is made by a task posted to the GUI event loop, with no wait. If the GUI thread is blocked in a call that waits
  for the JavaScript thread, a blocking hop would deadlock. When the observer exists it delivers any change it finds against
  the cache.
- `invalidate()` on the JavaScript thread clears the active flag and posts the observer's destruction to the GUI thread.
- The non-`jsThread` path keeps its synchronous construction and its owner-thread checks.

The default-off flags stay off: `RNQNX_ENABLE_APP_STATE`, `RNQNX_ENABLE_WINDOW_CHROME`, `RNQNX_ENABLE_INPUT_LIFECYCLE` and
`Options::jsThread`. The AppState receipt and the "chrome operable while JavaScript is blocked" receipt come from one build with
all four on, since that build is the one the Pi 400 runs.

### What restore means

Restore is the one-instance socket from ADR-0016. The key is the base app id, without build-variant suffixes. An app that supplies
`--app-id` keeps its own id. An app without an identity, the playground, keys on `org.rnqnx.playground`
(`rnqnx::runtime::defaultApplicationId()`), so `--restore-only` and `--stop-running` find the running copy. The 2026-11-23 Pi 400
run started a second playground because that key did not exist. One-shot `--screenshot` and `--timing-json` runs of an app without
an identity do not take the key.

### What a minimized process may and may not do

A minimized process is a running process that has no visible window. It may keep its JavaScript runtime, timers, network
connections and React state, and it receives `background` once. It may not count on being suspended, kept alive by the platform, or
woken: QNX Screen gives it no background-execution grant, and this ADR adds none. It cannot draw, receive input or take focus
until restored. Work that must survive the process belongs to the app's own persistence. Under `fullscreen-winmgr` the only way
back is a second launch, so an app that exits while minimized loses its way back except through its launcher tile.

## Consequences

- Minimize and restore are visible to JavaScript on both window managers without trusting a Qt signal that may not fire.
- A new state source sits beside Qt's: a Qt `Active` can end a host minimize, which keeps a window-manager restore from leaving
  JavaScript at `background` for good.
- `HostWindowState` is process-wide, so two windows in one process would share it. The windows API states single-window scope
  for v1 and the state follows it.
- AppState works with JavaScript on its own thread. Its first value there comes from a one-word mirror of Qt's state, and a change
  that lands before the observer exists is corrected when it attaches.

## Alternatives considered

- **Rely on `applicationStateChanged`.** Rejected: the destroyed platform window may produce no event, which is what ADR-0016 found.
- **Suspend or `SIGSTOP` the process on minimize.** Rejected: it stops timers and sockets, and QNX gives no resume path that
  `fullscreen-winmgr` understands.
- **Block the JavaScript thread on a GUI-thread hop to build the observer.** Rejected: the GUI thread blocks in
  `jsThread_->call` while the host loads, so the hop deadlocks.
- **Have the module own a `QWindow` listener.** Rejected: the window code already knows when it minimizes, and the module would
  need the window handle.

## Related files

- `native/lifecycle/HostWindowState.h`, `native/lifecycle/HostWindowState.cpp`
- `native/lifecycle/QtApplicationLifecycle.h`, `native/lifecycle/QtApplicationLifecycle.cpp`, `native/lifecycle/LifecycleTest.cpp`
- `vnext/src/runtime/QnxAppStateModule.cpp`, `vnext/src/runtime/QtReactHost.cpp`
- `native/window-chrome/WindowHost.cpp`, `native/window-chrome/WindowHost.h`, `native/window-chrome/WindowChromeQuickTest.cpp`
- `vnext/include/react-native-qnx/runtime/ApplicationIdentity.h`, `vnext/probes/playground/main.cpp`
- `cmake/RnqnxAppState.cmake`, `cmake/RnqnxWindowChrome.cmake`

## Notes

Evidence so far, all macOS host (Qt 6.12.0, offscreen platform): lifecycle observer tests including host minimize and restore,
Qt reports during minimize, repeated minimize, and the atomic state mirror read from another thread; the window-chrome quick test
for the minimize and restore notifications and for the one-instance key; the application identity test for the default key.
`QnxAppStateModule.cpp` compiles on the host with `-fsyntax-only`.

Needed before Accepted:

1. QNX VM: the `appstate` runtime test (`RNQNX_BUILD_APPSTATE_TESTS`) with `jsThread` on, for the module's JavaScript-thread path.
2. Pi 400 under `fullscreen-winmgr`: minimize, see `background` logged, relaunch with `--restore-only`, see `active` logged, with
   the chrome-while-JavaScript-blocked receipt from the same build.
3. Pi 400 or the QNX VM under `rnqnx-winmgr`: the same with the manager message.
