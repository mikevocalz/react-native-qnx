# S3 follow-up receipt: the tree beacon (2026-11-21)

Follow-up to `docs/evidence/s3-tree-wait-2026-11-20.md`, which left it open:
`rnqnx_wait_node` waits for `RNQNX_NODE name=<objectName>` and no app printed
that line. Base `fb20c3ad`. **No gate closes.** `docs/releases/v1-readiness.json`
is unchanged. Every result carries its target label. "Host" is a macOS arm64
laptop (Qt 6.12.0, Apple clang 21).

## What changed

| Piece                                                                          | Where                                     |
| ------------------------------------------------------------------------------ | ----------------------------------------- |
| `rnqnx::window::NodeBeacon`, prints `RNQNX_NODE name=<n> visible=1`            | `native/window-chrome/NodeBeacon.{h,cpp}` |
| Host test, 6 cases                                                             | `native/window-chrome/NodeBeaconTest.cpp` |
| Playground host creates the beacon for `rnqnxWindowChrome` and `rnqnxTitleBar` | `vnext/probes/playground/main.cpp`        |
| `pi-mouse-smoke.sh` waits for `rnqnxTitleBar` before it injects input          | `scripts/pi-mouse-smoke.sh`               |
| Shell test for that wait, with a mutation                                      | `tests/js/qnx-tree-wait.test.mjs`         |

The beacon scans its root on every `QQuickWindow::frameSwapped`, prints one
line per watched name the first time a node with that `objectName` is
`isVisible()`, flushes stdout, and stops reporting that name. The signal can
come from the render thread, so the connection is queued onto the thread that
owns the beacon. It does not link `rnqnx_native_testing`; the S3 contract keeps
that library out of apps.

## Results

### Host: C++ beacon (macOS arm64, Qt 6.12.0)

Red first: with a stub `NodeBeacon` that prints nothing, `rnqnx_node_beacon`
failed `reportsAVisibleNodeOnceAfterAFrame`, `staysSilentForANodeThatIsNotThere`
and `reportsANodeThatAppearsLater` (`Actual (lines.size()): 0`, `Expected (1)`)
and then hit the 60 s CTest timeout.

Green with the implementation, plain Debug and ASan+UBSan (clang++), both with
`-Wall -Wextra -Wpedantic -Werror` on the library target:

```text
100% tests passed out of 16
```

16 is the whole `native/window-chrome` CTest set (15 existing plus
`rnqnx_node_beacon`). The beacon test covers: a visible node reported once and
not again, a node hidden by its parent never reported, a missing node never
reported, a node created by a `Loader` after the first frames reported when it
appears, the root matching, no call after the beacon is destroyed, and the real
`WindowChrome` reporting `rnqnxWindowChrome` and `rnqnxTitleBar`.

Mutation: removing the `isVisible()` check from the scan fails
`staysSilentForANodeThatIsNotThere` (`Actual (lines.size()): 2`, `Expected (1)`).
The check was restored and the test passed again.

### Host: shell wait against a synthetic target (`tests/js/qnx-tree-wait.test.mjs`)

The test failed first: with a log that has no `RNQNX_NODE name=rnqnxTitleBar`
line, the old `pi-mouse-smoke.sh` ran anyway (`mouse smoke goes red when the
title bar node line never arrives`). With the change it exits non-zero, injects
nothing and prints `timed out after 2s waiting for node 'rnqnxTitleBar'`. The
green runs log the node line first and pass. `shellcheck -x` and `bash -n` on
the script pass.

### QNX VM: `rnqnx_wait_node` in the target `sh` (QNX 8.0.0 aarch64le, QEMU)

`scripts/lib/wait-log.sh` was copied to the VM and run in its `sh` against a log
file. This closes the receipt row "not run on the QNX `sh`" for this function,
not for the other scripts.

| Case                                                   | Result                             |
| ------------------------------------------------------ | ---------------------------------- |
| `rnqnxTitleBar` present                                | return 0                           |
| `rnqnxTitle` (prefix of a node)                        | return 124, message names the node |
| `nope` (absent)                                        | return 124, message names the node |
| `late`, appended to the log 2 s after the wait started | return 0                           |

## Not run

| Check                                               | Status                                                                                     |
| --------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| Playground host compiled with the beacon            | **not run locally**: no Hermes/RN host tree on this laptop. The hosted host jobs build it. |
| Beacon in a QNX build, on the QNX VM or Pi 400      | **not run**: nothing here cross-builds the playground for QNX                              |
| `pi-mouse-smoke.sh` on the Pi 400                   | **not run: no board**                                                                      |
| Release board                                       | **not run**                                                                                |
| Threaded render loop on QNX (queued `frameSwapped`) | **not run**; the host offscreen test uses the software backend                             |

## Rollback

Revert the PR. Nothing outside `native/window-chrome`, `cmake/RnqnxWindowChrome.cmake`,
`vnext/probes/playground/main.cpp`, `scripts/pi-mouse-smoke.sh`, `tests/js`, the
generated `docs-site` index and the two doc lines.
