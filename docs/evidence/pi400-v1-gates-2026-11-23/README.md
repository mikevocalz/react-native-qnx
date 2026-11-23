# Pi 400 device receipts from `main` 43056487, 2026-11-23

Labels: **Pi 400** is the board at `qnxuser@192.168.2.2` (QNX 8.0.0 `RaspberryPi400 aarch64le`, 4 x Cortex-A72 at
1800 MHz, 4032 MB). **container** is a new Linux x86_64 container (`ubuntu:24.04`, QNX SDP 8.0.5 `q++` 12.2.0, 6 CPUs).
**host** is the Mac. Repository base: `43056487822accb3f6d00bf2d5745f30015e3aa2`, plus two local changes listed under
[Changes in this branch](#changes-in-this-branch).

**No gate changed.** `window-chrome`, `app-identity`, `performance` and `expo` stay `pending`.
`docs/releases/v1-readiness.json` and `docs/releases/performance-budgets.json` are untouched. Each gate's missing criteria
are listed below. The Pi 400 is not the stock Pi 4 QSTI or the release board, so none of this counts for `device-parity`
or `cold-boot`.

## What was built

| Step                                                                                              | Where     | Result                                                                                                                                                                                                                                                                                               |
| ------------------------------------------------------------------------------------------------- | --------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Qt 6.12.0 for QNX aarch64le, single pass from an empty tree, `JOBS=2`                             | container | extract, CVE and QNX patches, host tools (4039 s), Qt core (3299 s), qtsvg (87 s), qtimageformats (95 s): every stage exit 0 ([build/qt-stages.tsv](build/qt-stages.tsv)). About 2 h 6 min wall, longer than the 74 min of the 2026-11-23 distribution receipt because of `JOBS=2` against `JOBS=4`. |
| Closure of the staged runtime                                                                     | container | `verify-closure.py --runpath-prefix /data/qt612-public/` `RESULT PASS`, all seven checks ([build/closure.json](build/closure.json)); 188 file hashes in [build/runtime-files.sha256](build/runtime-files.sha256).                                                                                    |
| Runtime copied to the Pi 400                                                                      | Pi 400    | unpacked to `/data/qt612-public`; `sha256sum -c` read 188 OK, 0 failed.                                                                                                                                                                                                                              |
| `rnqnx_playground_qnx`, Release, `-DRNQNX_ENABLE_WINDOW_CHROME=ON -DRNQNX_ENABLE_TIMING_PROBE=ON` | container | 429 steps, exit 0, `DT_RUNPATH /data/qt612-public/lib`, SHA-256 `746eee21...` ([build/app-build.log](build/app-build.log)). First link of the timing probe's QNX branch (`QnxProcessMemory.cpp`).                                                                                                    |
| Same plus `-DRNQNX_ENABLE_APP_STATE=ON`                                                           | container | exit 0, SHA-256 `d0083ac0...` ([build/app-build2-appstate.log](build/app-build2-appstate.log)).                                                                                                                                                                                                      |
| Bundles (`--platform qnx --dev false`, host `hermesc` 260318099.0.5 `-emit-binary`)               | host      | hashes in [board/deployed-sha256.txt](board/deployed-sha256.txt).                                                                                                                                                                                                                                    |

Hermes is the existing QNX build in `~/qnx-pi400-8.0.5/pr102-candidate/build/hermes-qnx-aarch64` (`libhermesvmlean.so`,
`c3e02a14...`, the same hash as the 2026-11-22 and 2026-11-23 distribution receipts), with the prepared source in
`rn-source/third-party/hermes-260318099.0.5` (same commit `9a115f15`). It was not rebuilt. React Native third-party sources
came from `/Volumes/QNX/clones/fc`. The QNX licence was supplied as `docs/packaging.md` describes; contents were not read,
logged or hashed.

Host disk guard: floor 8 GiB, sampled every 30 s ([build/internal-free.tsv](build/internal-free.tsv)); the lowest sample was
8,738,288 KiB (8.33 GiB) and the guard never fired.

## Performance (`performance`)

Pi 400, `rnqnx_playground_qnx` `746eee21...` (timing probe on), 20 sequential runs per workload, one process per run, with no
warm-up run discarded (the first run is in the set). Display: the Screen framebuffer at 1280x720 window size; HDMI output is
black, so the display refresh was **not** measured. Raw files are in [perf/](perf/). `node scripts/validate-budgets.mjs --derive`
([perf/derive.json](perf/derive.json)) gives:

| Workload                                                                    | Runs | Startup to first frame, ms (min / p50 / max) | Frame interval p95, ms (p50 of runs) | Frame interval max, ms (p50 of runs) | Peak RSS, KiB (min / p50 / max) |
| --------------------------------------------------------------------------- | ---- | -------------------------------------------- | ------------------------------------ | ------------------------------------ | ------------------------------- |
| Hello World (`apps/performance-lab/hello-world.js`)                         | 20   | 253.0 / 463.2 / 790.1                        | not applicable (one frame)           | not applicable                       | 107232 / 107260 / 107400        |
| 1000-row list (`apps/performance-lab/index.js`), 40 wheel notches at 100 ms | 20   | 405.4 / 480.5 / 619.6                        | 41.4 (range 38.3 to 43.7)            | 49.1 (range 44.8 to 56.7)            | 127292 / 128324 / 129144        |

Every run had `droppedFrames` 0, `renderBackend` `opengl`, and the list runs had 226 to 255 swaps. A swap is the end of a
scene-graph swap, not a display presentation. RSS comes from the new `DCMD_PROC_PAGEDATA` sampler every 100 ms
(`rssSource` `sampled-procfs`); it was checked against allocations in
[`pi400-timing-reader-2026-11-23`](../pi400-timing-reader-2026-11-23/README.md) but not against a second OS source.
Hermes ran as an interpreter (`HERMESVM_ALLOW_JIT=0`).

**Not met:** the third workload, drawer drag, is blocked in `apps/performance-lab/fixtures.js` (no qualified gesture and
Worklets path), so no number exists for it. The acceptance text asks for all three. Frame intervals use wheel input posted by
the probe, not a physical touch or mouse drag. The Pi 400 fields in `performance-budgets.json` are still `unmeasured`: filling
them is a budget change for the owner and was not made here. The refresh rate was not read from the display.

## Window chrome (`window-chrome`)

Pi 400, under `rnqnx-winmgr`, `--window-controls macos` and `windows`, default title bar style.

- Both control layouts render on the board: [macos](frames/playground-macos-layout.png) (close and minimize at left; maximize is
  hidden because the app has no initial size) and [windows](frames/playground-windows-layout.png) (minimize and close at right),
  with the title `react-native-qnx playground` and an inset of 44 logged
  ([board/playground-macos.log](board/playground-macos.log), [board/playground-windows.log](board/playground-windows.log)).
- Minimize: a click on the minimize control through `rnqnx-inject5` logged `RNQNX_WINDOW_ACTION minimize`, and the next board
  screenshot showed the desktop without the window ([frames/after-minimize-launcher.png](frames/after-minimize-launcher.png)).
  The process stayed alive. **No AppState change was logged** ([board/chrome-probe-macos-minimize.log](board/chrome-probe-macos-minimize.log)
  and [board/chrome-probe-windows-minimize.log](board/chrome-probe-windows-minimize.log) hold only `inactive` then `active` at
  start), with the AppState-enabled build `d0083ac0...`.
- Restore did not work as a receipt. `--restore-only` started a second copy of the app instead of showing the first, because the
  playground binary has no application identity to key the one-instance socket on. I stopped both copies with SIGTERM. For
  a minute two copies ran on the board during this test; they did not coexist at any other time.
- The first build, without `RNQNX_ENABLE_APP_STATE`, ended the probe with `Cannot use AppState when isAvailable is false`
  ([frames/chrome-probe-js-fatal-appstate-off.png](frames/chrome-probe-js-fatal-appstate-off.png)). A build for the gate needs that
  option on.
- Chrome while JavaScript is blocked: not shown. Injected clicks on the page content (the `Block JS` button) did not reach the app,
  so the probe could not be triggered from the board. The Qt-linked VM run in `s2-vm-2026-11-21` remains the only receipt.

**Not met:** the Nitro `QnxWindows` windows API (title, size, position, minimize, close, close guards, focus and visibility
events) is not implemented, so a close guard that cancels, one that proceeds, and the inset reaching JavaScript and
`SafeAreaView` have no surface to test. There is no per-target `startSystemMove`/`startSystemResize` receipt, no ADR for
minimize on QNX Screen that matches an AppState transition (ADR-0016 describes minimize as destroying the native window and
restoring on relaunch), no AppState transition on minimize and restore, and no blocked-JS receipt.

## App identity (`app-identity`)

Not met. The playground binary is not an app prepared with `prepare-native`, so it carries no generated icon set. The task
switcher in [frames/chrome-probe-appstate-active-macos.png](frames/chrome-probe-appstate-active-macos.png) shows the generic `R`
tile and the executable name `rnqnx_playground_qnx` truncated to `rnqnx_playground_qnx_` for the `_as` binary. The
launcher's `QNX Components` tile (old showcase build, [frames/chrome-probe-js-fatal-appstate-off.png](frames/chrome-probe-js-fatal-appstate-off.png))
still shows the square dark icon background. No custom icon, label box measurement or tile grid was produced. Needs a
`prepare-native` app built against this runtime and installed as a tile.

## Expo (`expo`)

`apps/expo-router-example` (bundle `aef7ee44...`, 2,186,824 bytes of bytecode) ran on the Pi 400 in the playground runner and
drew the Router Example Home screen with its three tabs ([frames/expo-router-example-home.png](frames/expo-router-example-home.png),
[board/expo-router-example.log](board/expo-router-example.log)). The log shows `No native ExponentConstants module found`:
this runner is the plain playground, not an Expo-prepared app with autolinked modules. Injected clicks on the tabs did not change
the screen, so tab navigation was not shown.

**Not met:** everything else in the gate. No downloaded runner with a published hash, no create-and-run path through Expo CLI
with Fast Refresh on the board, no standalone package built with the SDP and launched, no native-module requirement check
before JavaScript loads on the board, and no recording. `gh release list` for the repository had no runner release on
2026-11-23.

## Changes in this branch

- `scripts/qt-public/build-app-qnx.sh` accepts `APP_CMAKE_EXTRA` (whitespace-separated `-D` options), used here for chrome, the
  timing probe and AppState.
- `apps/rn-playground/chrome-device-probe.js` logs AppState changes as `RNQNX_CHROME_PROBE` lines and blocks the JavaScript
  thread for 20 s on a press.

## Cleanup

Pi 400: app stopped, `pidin` shows no `playground_qnx` process. `drm-rpi4`, `screen`, `rnqnx-winmgr` and `/system/etc/startup`
were not touched. Left on the board: `/data/qt612-public` (verified runtime), `/data/home/qnxuser/v1g` (binaries, bundles,
launch scripts, listed in [board/deployed-sha256.txt](board/deployed-sha256.txt)), `/data/home/qnxuser/runtime-v1g*` and
files in `/tmp`. Nothing launches at boot.
