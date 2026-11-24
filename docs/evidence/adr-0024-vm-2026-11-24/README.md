# ADR-0024 on the QNX VM, 2026-11-24

**Target: QNX VM (QEMU virt aarch64le, TCG, software render).** `QNX qnxvm 8.0.0 2026/06/05-16:21:14EDT QEMU_virt aarch64le`
([guest-uname.txt](guest-uname.txt)). Not the Pi 400 and not the release board; TCG timings are not performance evidence. No gate in
`docs/releases/v1-readiness.json` changes.

## What ran

`qt_react_host_js_thread_qnx_test`, cross-built in the `qnx805-build` container from commit `9d1560b8` (this branch, which includes
the `HostWindowState` thread-affinity and observer-deleter changes), Release `-O2`, lean Hermes, Qt 6.12.0, with
`RNQNX_ENABLE_APP_STATE=ON`, `RNQNX_ENABLE_WINDOW_CHROME=ON`, `RNQNX_ENABLE_INPUT_LIFECYCLE=ON` ([build/build.sh](build/build.sh);
Worklets, Reanimated, Gesture Handler and Nitro were left out of this build). It was pushed to the guest with
`scripts/validate-qnx-vm.sh --stage --skip-captures`, and host and guest hashes matched ([stage-js-thread.host.sha256](stage-js-thread.host.sha256),
[stage-js-thread.guest.sha256](stage-js-thread.guest.sha256)). Binary sha256 `ec7672ee...8b`.

## Result

The binary exited 0 and printed `All 10 JS-thread tests passed.` ([js-thread/run.txt](js-thread/run.txt),
[js-thread/stdout.txt](js-thread/stdout.txt)). The AppState lines, with JavaScript on its own thread and blocked for 5 s:

```text
[RESULT] AppState initial=active
[RESULT] AppState transitions after the block: background,active
[RESULT] AppState resolves in JS-thread mode: ok
```

The test minimizes and restores the window (`HostWindowState`) while JavaScript is blocked. The two transitions were queued behind
the block and delivered in order once it ended. The chrome stayed operable during the block (`minimizeMs=19 closeMs=10 resizeMs=0`,
`heartbeats@2501ms=190`). Four window grabs are in [js-thread/shots](js-thread/shots).

## What this does not show

- `scripts/validate-qnx-vm.sh` exited 1 and [result.json](result.json) says `fail`: the script requires at least 11 `[PASS]`
  lines, and the eleventh is a Nitro check that this build does not contain. The threshold was not changed. The 10 lines that
  exist all passed and there is no `[FAIL]` line.
- This is the JS-thread test's AppState coverage, not `rnqnx_appstate_runtime_test`. That test needs `BUILD_TESTING` on a native
  Linux graph (`cmake/RnqnxAppState.cmake`) and has no JS-thread mode, so it does not run on the VM. Its Linux ASan and UBSan lane
  is `runtime-appstate.yml`.
- `js task error: AppRegistryBinding::stopSurface failed. Global was not installed.` appears during fixture teardown. The same
  line is in the Pi 400 run ([js-thread-test-pi400.txt](../pi400-v1-gates-2026-11-24/board/js-thread-test-pi400.txt)); the test
  passes with it.
- Nothing ran under the stock `fullscreen-winmgr`. See ADR-0024, Notes.

Verify the directory with `shasum -a 256 -c SHA256SUMS`. `result.json` was reformatted with Prettier after the run (whitespace only) and its checksum updated.
