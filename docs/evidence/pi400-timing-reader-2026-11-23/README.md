# QNX resident-set reader for the timing probe, 2026-11-23

Labels: **Pi 400** is the board at `qnxuser@192.168.2.2`, QNX 8.0.0 `RaspberryPi400 aarch64le`.
**container** is a throwaway Linux x86_64 container with QNX SDP 8.0.5 `q++`. **host** is macOS.

**No gate changed.** `window-chrome`, `app-identity`, `performance` and `expo` stay `pending`.
`docs/releases/v1-readiness.json` and `docs/releases/performance-budgets.json` are untouched.

## Why there is no QNX build of `main` here

The run needed a full Qt 6.12 build for QNX (about 74 minutes with `JOBS=4` last time) before
any app could link. It started at 2026-11-23T15:25:42Z with the licence supplied as in
`docs/packaging.md`. The extract, patch and host-tools stages began. At 15:27:03Z the guard
read 7.1 GiB free on the internal disk (floor: 8 GiB) and stopped the build. I had cleared the
npm and pnpm caches first. The disk stayed between 7.5 and 7.7 GiB for the next ten minutes with
nothing running of mine, and `sysctl vm.swapusage` showed about 9.5 of 10 GB of swap in use, which
matches the swap pattern in the 2026-11-23 distribution receipt. `~/.gradle/caches` (2.5 GB) is
left because `rm -rf` on it was denied. No Qt tree, runner, `rnqnx_playground_qnx` or prepared app
was built, so nothing from `main` ran on the Pi 400 and no old binary is presented as evidence.

## What was written

- `vnext/probes/timing/QnxProcessMemory.{h,cpp}`: resident set from `DCMD_PROC_PAGEDATA` on
  `/proc/self/as` (sum of runs with `PG_HWMAPPED`), and `PeakTracker`.
- `QnxTimingSource.cpp` uses it. `TimingProbe.cpp` has a QNX branch: it samples every 100 ms
  while running, reports the highest sample as `peakRssKiB` and sets `rssSource` to
  `sampled-procfs`, `os` to `qnx`. CMake accepts `RNQNX_ENABLE_TIMING_PROBE=ON` on QNX for
  `rnqnx_playground_qnx`.
- The frame-swap hook is the existing `QQuickWindow::frameSwapped` connection. Nothing QNX
  specific was needed. It has not run on QNX.
- Host test: `tests/native/qnx-timing-reader` gained cases for the run sum, mask, rounding,
  overflow and peak tracking. `ctest` passed on macOS (1 of 1).

## Reader run on the Pi 400

`drv.cpp` (this folder) links `QnxTimingSource.cpp` and `QnxProcessMemory.cpp`, built with
`q++ -Vgcc_ntoaarch64le -std=gnu++20 -O2 -Wall -Wextra -Werror`, exit 0. It reads the resident set,
allocates and touches N MiB, reads again, frees, reads again.

| Run     | before    | after touching N MiB | delta       | after free | peak       |
| ------- | --------- | -------------------- | ----------- | ---------- | ---------- |
| 100 MiB | 3248 KiB  | 105652 KiB           | 102404 KiB  | 3248 KiB   | 105652 KiB |
| 300 MiB | 3248 KiB  | 310452 KiB           | 307204 KiB  | 3248 KiB   | 310452 KiB |

The deltas equal the allocation plus 4 KiB, so the reader tracks touched pages. The clock rate
read from the syspage was 54,000,000 Hz. `getrusage` `ru_maxrss` was 0 in an earlier probe, which
is why the peak is sampled. The reader was not compared with a second OS source: `pidin mem`
lists mappings, not a resident total. The program was removed from the board and `pidin` showed no
copy. `drm-rpi4`, `screen` and `/system/etc/startup` were not touched.

## Not done

- The Qt-linked probe has not been compiled for QNX or run, so no startup, frame interval or
  RSS number exists for any workload. Every Pi 400 budget field stays `unmeasured`.
- The windows API (`QnxWindows`) is not implemented, as instructed. The inset-to-JavaScript
  test needs a JavaScript surface that does not exist yet and was not written.
- `expo` and `app-identity` device runs need the same QNX build.

## Cleanup

Container `qt-gates-c`, the licence copy, the 100 GB sparse image and the guard script are deleted.
`qnx805-build`, `rnqnx-qualify-20261122`, `a8-pr1-61315`, PRs #218 to #220 and
`rnqnx-software-20261122` were not touched. The clone is deleted after the push.
