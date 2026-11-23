# Performance: published host baseline

Date: 2026-11-22. This page publishes the host numbers already committed in
`docs/releases/performance-budgets.json`. The `performance` gate stays `pending`.
**No number here was measured on a QNX VM, a Pi 400 or the release board.** Every
QNX VM, Pi 400 and release board budget is `unmeasured` and holds no value.

The gate asks for startup to first frame, presented-frame intervals and RSS for Hello
World, a 1000-row list and drawer drag. This page covers the host lane for two of the
three workloads. It does not close the gate.

## Revision and run

| Item              | Value                                                                                            |
| ----------------- | ------------------------------------------------------------------------------------------------ |
| Lane label        | host                                                                                             |
| Source revision   | `cc20adfe591e0d3726e8a2ba507afc85afe3d3a0`                                                       |
| CI run            | GitHub Actions 44849611367, workflow `performance-budgets.yml`, conclusion success               |
| Runner            | ubuntu-24.04, 4 CPUs, Intel Xeon Platinum 8573C                                                  |
| Qt and backend    | Qt 6.11.1, offscreen platform, software scene graph backend                                      |
| Build             | Release, base runtime (feature flags OFF), `RNQNX_ENABLE_TIMING_PROBE=ON`                        |
| JavaScript engine | Hermes with `HERMESVM_ALLOW_JIT=0`, an interpreter                                               |
| Raw results       | `docs/evidence/s4-budgets-2026-11-20/ci-host-run-44849611367/` (44 files, bundle hashes there)   |
| Bundles           | `hello-world.js.bundle.js` sha256 `5e29d392...a0a7`, `index.js.bundle.js` sha256 `09c3d563...9e` |

## Sample sizes

| Workload      | Runs | Dropped frames | Swaps stored per run | Swaps inside the wheel window    |
| ------------- | ---- | -------------- | -------------------- | -------------------------------- |
| Hello World   | 20   | 0              | 2                    | not applicable (no wheel script) |
| 1000-row list | 20   | 0              | 264 to 270           | 254 to 259                       |

The list workload posts 40 wheel notches 100 ms apart after a 500 ms settle. The
validator needs at least 30 swaps in the window and rejects a run with dropped frames.

## Host baseline

Medians below are the p50 over the 20 runs. The ceiling is `ceil(max * 1.5)`. The factor
1.5 is a method choice, not a measurement.

| Workload      | Metric                                        | min   | p50   | max   | Ceiling |
| ------------- | --------------------------------------------- | ----- | ----- | ----- | ------- |
| Hello World   | startup to first frame (ms)                   | 100.1 | 101.3 | 103.4 | 156     |
| Hello World   | peak RSS (KiB)                                | 71264 | 71652 | 71840 | 107760  |
| 1000-row list | startup to first frame (ms)                   | 157.7 | 160.1 | 164.7 | 248     |
| 1000-row list | frame interval p95 over the wheel window (ms) | 18.49 | 18.65 | 19.54 | 30      |
| 1000-row list | frame interval max over the wheel window (ms) | 19.72 | 21.63 | 21.92 | 33      |
| 1000-row list | peak RSS (KiB)                                | 96248 | 96596 | 96908 | 145362  |

Hello World has no frame-interval budget because it does not animate. Drawer drag is
`blocked` on the host: it needs the qualified Gesture Handler and Worklets path, and a
scripted position change would not qualify it.

## Capture limitations

- A swap is the end of a scene-graph swap on the offscreen platform. It is not a display
  presentation, so the frame intervals are not presented-frame intervals.
- Startup runs from the first statement of `main()` to the first `frameSwapped` after the
  surface started. It excludes dynamic loading before `main()`.
- The offscreen platform with the software backend says nothing about the QNX Screen
  platform, the Pi 400 GPU path or the release board.
- Hermes runs without a JIT in every build here. A budget would need a new measurement and
  an ADR if that changes.
- The probe leaves the 16 ms microtask drain timer in place, so its quantisation is part of
  the measured startup.
- The probe was measured without an ON/OFF overhead comparison. Numbers recorded with it on
  carry that caveat.
- The runner is shared hardware. The ceiling factor leaves room for that noise.

## QNX timing reader

The budgets file says the QNX reader and clock mapping come with S9. Part of it now exists
as code, and none of it has run on QNX.

| Piece                                              | Path                                      | Status                                          |
| -------------------------------------------------- | ----------------------------------------- | ----------------------------------------------- |
| Tick to nanosecond mapping, origin handling        | `vnext/probes/timing/TimingReader.h`      | host tests pass (ASan and UBSan)                |
| Window selection, nearest-rank p95 and max         | `vnext/probes/timing/TimingReader.h`      | host tests pass; repeats `validate-budgets.mjs` |
| Counter source interface                           | `vnext/probes/timing/QnxTimingSource.h`   | host tests build against it                     |
| QNX source (`ClockCycles`, `SYSPAGE_ENTRY(qtime)`) | `vnext/probes/timing/QnxTimingSource.cpp` | **not built, not run**; guarded by `__QNX__`    |
| Host test                                          | `tests/native/qnx-timing-reader/`         | CI step in `performance-budgets.yml`            |

Not written: a `frameSwapped` hook for QNX (`TimingProbe.cpp` stays host-only and CMake
still rejects `RNQNX_ENABLE_TIMING_PROBE` on QNX), and an RSS reader for QNX
(`residentKiB` returns nothing there). Until both exist and run on a target, no QNX field
can leave `unmeasured`.

## Not covered

QNX VM, Pi 400 and release board numbers for all three workloads; drawer drag on any lane;
presented-frame intervals; any run with the Qt feature flags ON.
