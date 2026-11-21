# S2 receipt: JavaScript on its own thread

Date 2026-11-21. Branch `s2-js-thread`, PR #201. Decision record:
`docs/adr/0021-javascript-thread-off-the-qt-gui-thread.md`.

**Target: host only.** macOS arm64, Apple clang, Qt 6.12.0 (Homebrew), Hermes
Debug from the pinned source, `QT_QPA_PLATFORM=offscreen`,
`QT_QUICK_BACKEND=software`, `RNQNX_ENABLE_WINDOW_CHROME=ON`. No QNX VM, Pi 400
or release-board run exists; the Pi and the T9 drive were not available for a
device run. The `window-chrome` and `device-parity` gates stay `pending`.
A later QNX VM run (TCG, software render, not the Pi) is in
[s2-vm-2026-11-21](../s2-vm-2026-11-21/README.md); the items below were not run
in this host receipt, and the VM receipt says which of them it then ran.

## Commands

```sh
# ASan build (RNQNX_ENABLE_HOST_SANITIZERS=ON)
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DRNQNX_REQUIRE_FULL_HOST_RENDERER=ON -DRNQNX_BUILD_HERMES=ON \
  -DRNQNX_FETCH_REACT_NATIVE_DEPS=ON -DRNQNX_ENABLE_HOST_SANITIZERS=ON \
  -DRNQNX_ENABLE_WINDOW_CHROME=ON -DCMAKE_CXX_SCAN_FOR_MODULES=OFF
ninja -C build/host qt_react_host_js_thread_host_test qt_react_host_host_test
ASAN_OPTIONS=detect_leaks=0 build/host/bin/qt_react_host_js_thread_host_test -platform offscreen
ASAN_OPTIONS=detect_leaks=0 build/host/bin/qt_react_host_host_test -platform offscreen

# ThreadSanitizer build (everything compiled with -fsanitize=thread, ASan off)
cmake -S . -B build/tsan -G Ninja ... "-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer" \
  "-DCMAKE_C_FLAGS=-fsanitize=thread -fno-omit-frame-pointer" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread" \
  -DRNQNX_ENABLE_HOST_SANITIZERS=OFF -DRNQNX_ENABLE_WINDOW_CHROME=ON
TSAN_OPTIONS=halt_on_error=0 build/tsan/bin/qt_react_host_js_thread_host_test -platform offscreen
```

## Results

| Run                                                                       | Result                                                                                      | Log                       |
| ------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- | ------------------------- |
| JS-thread test, ASan                                                      | 6/6 pass; `blocked=5003ms beats@2500ms=234 total=484`                                       | `asan-run.txt`            |
| JS-thread test, ThreadSanitizer                                           | exit 0, 0 reports                                                                           | `tsan-run.txt`            |
| `qt_react_host_host_test`, flag off, ASan                                 | `All 62 QtReactHost host tests passed.`                                                     | not kept (run in session) |
| Mutation: blocked case with `Fixture(false)`                              | `[FAIL] blockedJavaScriptLeavesTheGuiThreadResponsive: ... check failed: !blockDone.load()` | not kept (run in session) |
| Mutation: acquire removed from `QueuedCallTarget::event`, ThreadSanitizer | 42 reports                                                                                  | not kept (run in session) |
| Earlier ThreadSanitizer run, before `QueuedCallTarget`                    | 93 reports, all the callable crossing Qt's queue                                            | not kept (run in session) |

## What the blocked case checks, while JavaScript spins for 5 s

- Press, drag and release on a mounted `Flickable` moves `contentY` by more than 10 px.
- Real presses on the title bar `minimize` and `close` controls fire `actionTriggered`; close hides the window.
- `resizeSurface` and `setWindowSize` return in under 500 ms; afterwards JavaScript has the new window size and the surface is laid out at it.
- A 10 ms heartbeat timer keeps at least a quarter of its rate to mid-block.
- Work queued behind the block runs after it.

## Not run

QNX VM, Pi 400, release board; Linux ThreadSanitizer (the workflow
`.github/workflows/js-thread-tsan.yml` is manual dispatch); Nitro installers,
dev client, Worklets, Reanimated, Gesture Handler and AppState in this mode;
touch events reaching JavaScript during a block; the frame-time cost of the
extra mount hop on any target.
