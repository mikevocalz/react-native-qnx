# S2 on the QNX VM: JavaScript off the GUI thread, 2026-11-21

**Target: QNX VM (QEMU 11.1.3, TCG emulation, software render). Not the Pi 400,
not the release board.** TCG timings are not performance evidence and are not
compared with any S4 budget. No gate status changes: `window-chrome`,
`device-parity` and `cold-boot` stay `pending`.

Decision record: [ADR-0021](../../adr/0021-javascript-thread-off-the-qt-gui-thread.md).
Host receipt this extends: [s2-js-thread-2026-11-21](../s2-js-thread-2026-11-21/README.md).

## What ran

| Item     | Value                                                                                                                                                                               |
| -------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Guest    | `QNX qnxvm 8.0.0 2026/06/05-16:21:14EDT QEMU_virt aarch64le` ([build/vm-uname-pidin.txt](build/vm-uname-pidin.txt)), 4 CPUs, 3 GiB                                                  |
| Build    | aarch64le cross-build in the `qnx805-build` container, Release `-O2`, lean Hermes, Qt 6.12.0 ([build/cmake-options.txt](build/cmake-options.txt), [build/build.sh](build/build.sh)) |
| Extra    | Nitro was added with `EXTRA="-DRNQNX_BUILD_NITRO_QNX=ON -DRNQNX_ENABLE_NITRO_QT_DISPATCHER=ON -DRNQNX_NITRO_CORE_ROOT=<pinned nitro-core>"` passed to `build.sh configure`          |
| Binary   | `qt_react_host_js_thread_qnx_test`, the same source as the host test (`tests/cpp/QtReactHostJsThreadHostTest.cpp`); hashes in [build/deploy.sha256](build/deploy.sha256)            |
| Scripts  | `tests/fixtures/js-thread/*.js` compiled with the pinned `hermesc`, because the QNX Hermes only runs bytecode                                                                       |
| Runner   | `scripts/vm-proof/js-thread.sh`, which starts QNX Screen (software blitter, dummy WFD display) as `capture.sh` does                                                                 |
| Input    | `QMouseEvent`s sent to the window. The VM has no input device, so no real Screen pointer input was used                                                                             |
| Not used | The showcase app. `main.cpp` builds `QtReactHost` without the option and reads `host.runtime()` on the GUI thread, so no app binary runs in this mode yet                           |

## Acceptance: JS blocked 5 s

Three runs, 11 cases each, all pass ([vm/run1](vm/run1/run.log), [run2](vm/run2/run.log), [run3](vm/run3/run.log)).

| Flag on, while blocked    | Run 1 | Run 2 | Run 3 |
| ------------------------- | ----- | ----- | ----- |
| Block length (ms)         | 5019  | 5018  | 5018  |
| Pan: contentY, 0 to       | 48    | 48    | 48    |
| Pan duration (ms)         | 101   | 99    | 102   |
| Minimize control (ms)     | 11    | 12    | 12    |
| Close control (ms)        | 5     | 6     | 6     |
| resize + setWindowSize    | 0     | 0     | 0     |
| 10 ms heartbeats by 2.5 s | 196   | 198   | 197   |
| Queued work ran after     | yes   | yes   | yes   |

Minimize on QNX releases the native window (`WindowHost.h`), so the test checks
the window is hidden and shows it again. Close hides the window. Resize and
window-size calls returned at once and JavaScript was told the new size after
the block.

**Control, flag off, same block.** 0 heartbeats during the block, the first at
5008 ms. The pan was queued before the block and had not moved the Flickable
when the block ended. A close click queued before the block acted at 5003 to
5004 ms, after it. Chrome is not operable with the flag off.

**Touch during the block, flag on.** 10 events dispatched natively; after the
block JavaScript saw 2 starts, 2 ends and 2 moves, all timestamped after the
block ended. Fabric coalesces queued `touchMove` events per view, so six sent
moves arrive as two. The same counts hold on the host.

## Items S2 listed as not run

| Item                                  | Result on the QNX VM                                                                                                                                                                                                             |
| ------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Worklets, Reanimated, Gesture Handler | Each `install` threw `X is not supported with QtReactHost JS-thread mode yet (ADR-0021)`                                                                                                                                         |
| AppState                              | `nativeModuleProxy.AppState` threw `AppState is not supported with QtReactHost JS-thread mode yet (ADR-0021)`                                                                                                                    |
| Nitro installer                       | `installQnxNitroRuntime` ran on the JS thread with `QtNitroDispatcher`; an async callback from a worker and a native Promise settled from a worker both reached JavaScript on the JS thread. `runSync` from the GUI thread threw |
| WebSocket                             | Module built on the JS thread, transport on the GUI thread. Loopback echo server: open, send, echo, close all delivered, every event on the JS thread, 0 failures                                                                |
| FileReader                            | `readAsText` rejected with `FileReaderModule is not implemented on this platform` through a Promise settled on the JS thread                                                                                                     |
| Image modules                         | `ImageLoader.getSize` on a 37x23 PNG resolved `[37, 23]` after the thread-pool decode                                                                                                                                            |
| Touch events during a block           | See above                                                                                                                                                                                                                        |

## Not run

- Pi 400 and release board. Cold boots (S9) and the frame-time cost of the extra mount hop.
- Real Screen pointer or touch input; a person watching the display. The VM has no input device.
- The showcase app or any app binary in this mode.
- `QnxDevSupport` (development client), the Expo image view and the image modules beyond `getSize`, Nitro hybrid objects called from JavaScript, Gesture Handler recognizers in this mode (the library refuses to install).
- A Linux ThreadSanitizer negative control.

## Screenshots

[vm/shots](vm/shots) holds window grabs taken mid-block on the VM. `flag-on-after-pan-mid-block.png`
shows the title bar and the scroll view moved by 48 px. The two `restore` grabs
through the platform window are blank: the grab path returned an empty image after
the native window was recreated, and it was not investigated further. The
`item-tree` grab, which renders the item tree and does not use the platform
buffer, shows the title bar and the scrolled content intact after the restore.
The host grab is [host/flag-on-after-pan-mid-block-host.png](host/flag-on-after-pan-mid-block-host.png).

## Findings from getting here

- **ThreadSanitizer on Linux was red on main.** Run 44919044567 reported 21 races,
  each a Qt allocation freed on Qt's QML loader thread, with Qt not instrumented
  ([ci/tsan-main-5b691720.txt](ci/tsan-main-5b691720.txt)). Root cause: Linux
  defaults `ignore_noninstrumented_modules` to off, macOS to on, which is why the
  macOS receipt had none. The workflow now sets it. Run 44921312130 on commit
  `6d42c522`: success, 0 reports ([ci/tsan-branch-6d42c522.txt](ci/tsan-branch-6d42c522.txt)).
- The QNX Hermes cannot run JavaScript source, so the test reads bytecode.
- Fabric coalesces queued touch moves, so the touch assertion counts starts, ends and per-view moves.

## Host check of the changed test

[host/asan-run.txt](host/asan-run.txt): macOS arm64, Qt 6.12.0, ASan, offscreen, 9 cases, all pass.
The libraries that refuse and the WebSocket and Nitro cases are not compiled into the host target, so only the QNX run covers them.
