# Hermes build hygiene (S1), 2026-11-20

Base: `main` at `9fab2f18`. Every number is host evidence from one macOS
27.0 arm64 machine (Apple clang 21.0.0, CMake 4.4.5, Homebrew Qt 6.12.0, Debug).
Nothing here ran on a QNX VM, the Pi 400 or the release board. Other agent lanes
were building on the same machine, so wall times carry noise.

Two kinds of number appear. Where a table row says **re-measured**, the raw log
is committed in `hermes-build-hygiene-2026-11-20/`. Where it says **earlier run,
raw log lost**, the number was read from terminal output of a first session
whose build trees and logs were on an external drive that disconnected. Those
numbers are not gate evidence. They need a re-run on CI before anyone cites them.

## Why the earlier slow-debug change was backed out

`e6030bce` forced `HERMES_SLOW_DEBUG` off only when a musl loader existed. It
answered the abort `Assertion failed: bounds == detail::thread_stack_bounds_impl()`
about 8 s after launch on the Marathon Alpine host. `docs/evidence/marathon-vm-2026-11-20/README.md`
traces that abort to musl's `pthread_getattr_np`, which reports only the mapped
part of the main-thread stack. With the assert off, JS calls from the event loop
still failed with "Maximum call stack size exceeded (native stack depth)", so the
change hid the first symptom. `b6cfc738` reverted it 20 minutes later and `00ce1393`
added the root-cause fix, `native/platform/linux-host/MainThreadStack.hpp`, which
touches the stack down to `RLIMIT_STACK` before Hermes starts. The revert has no
message body; this account comes from the commit order and that README. The
Marathon musl host has not been re-run with slow debug off.

## Change

- `cmake/dependencies/Hermes.cmake`: force `HERMES_SLOW_DEBUG` OFF in the cache,
  append `-O2` to `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS` for the Hermes subtree
  only, and restore both right after `add_subdirectory`. The forced cache entry
  also overrides a stale `ON` in an existing build directory.
- `vnext/probes/hermes/build_mode.cpp` and the `rnqnx_hermes_build_mode` CTest:
  `HermesInternal.getRuntimeProperties().Build` must be `Debug` or `Release`.
  Hermes reports `SlowDebug` when `HERMES_SLOW_DEBUG` is defined
  (`lib/VM/JSLib/HermesInternal.cpp` at `9a115f15`).
- `scripts/validate-host-renderer.mjs`: the new test joins the expected list.

## Red, then green (re-measured)

One configure (`cmake -S . -B build/h -G Ninja -DCMAKE_BUILD_TYPE=Debug
-DBUILD_TESTING=ON -DRNQNX_REQUIRE_FULL_HOST_RENDERER=ON -DRNQNX_BUILD_HERMES=ON
-DRNQNX_FETCH_REACT_NATIVE_DEPS=ON`), then `ninja rnqnx_hermes_build_mode
rnqnx_hermes_probe` and `ctest -R rnqnx_hermes`. Both runs used the same build
directory; the fix was the only difference.

| State                            | Cache                        | Build (Hermes rebuilt) | `rnqnx_hermes_build_mode`                          | `rnqnx_hermes_probe` |
| -------------------------------- | ---------------------------- | ---------------------- | -------------------------------------------------- | -------------------- |
| Fix absent (`main` Hermes.cmake) | `HERMES_SLOW_DEBUG:BOOL=ON`  | 95 s                   | fails, `RNQNX_HERMES_BUILD_MODE SlowDebug`, 0.09 s | passes, 0.57 s       |
| Fix present                      | `HERMES_SLOW_DEBUG:BOOL=OFF` | 99 s                   | passes, `RNQNX_HERMES_BUILD_MODE Debug`, 0.04 s    | passes, 0.27 s       |

Logs: `before-ctest.log`, `after-ctest.log`, `after-cmake-cache-excerpt.txt`.
With the fix present `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS` are empty in the
cache and no compile line defines `HERMES_SLOW_DEBUG`
(`after-optimisation-levels.txt`: 490 Hermes compile lines at `-O2`).

The CI ctest run is the first one that covers the full renderer graph with this
exact diff.

## CTest wall time on the full graph (earlier run, raw log lost)

A session before this one built the whole host graph and ran CTest on it. Those
runs used the same change; the numbers below are not backed by a committed log.

| Leg                                                                                                                       | Before (SlowDebug, -O0)          | SlowDebug off, still -O0 | After (off, -O2)       |
| ------------------------------------------------------------------------------------------------------------------------- | -------------------------------- | ------------------------ | ---------------------- |
| 18 tests, no optional features                                                                                            | 57.1 s, reruns 52.6 s and 52.2 s | not run                  | 19.2 s, 13.1 s, 14.7 s |
| 60 tests (SVG, RNSVG, native UI, Expo image, Qt motion, graphics, video, input, text input, accessibility, window chrome) | 490.5 s                          | 262.1 s                  | 115.4 s                |
| `qt_react_host_host_test` (60-test leg)                                                                                   | 48.5 s                           | 48.1 s                   | 12.0 s                 |
| `rnqnx_qt_rnsvg_corpus_test_1`                                                                                            | 154.5 s                          | 23.2 s                   | 5.05 s                 |
| `rnqnx_qt_rnsvg_corpus_test_2`                                                                                            | 148.6 s                          | 32.2 s                   | 3.39 s                 |

The 60-test leg could not enable Worklets, Reanimated or Gesture Handler on
macOS: `generated/worklets/AsyncQueueImpl.qnx.cpp` calls `pthread_setname_np`
with two arguments, which the macOS SDK rejects. Those run on the Linux CI leg.
`rnqnx_expo_image_module_host_test` aborted with an ASan container-overflow in
`std::deque<QString>` (`DecodedImageCache.h:100`) in all three states, so it is
unrelated to Hermes. It was not investigated.

Turning slow debug off alone removed the corpus cost and left
`qt_react_host_host_test` at 48 s; only `-O2` took it to 12 s.

No time limit changed. The 300 s corpus-shard limit, the 60 s limits and the
180 s preset limit stay until Linux CI timings from this PR exist.

## Optimisation levels found

| Build                                                                             | Source of the reading   | Level                                                                  |
| --------------------------------------------------------------------------------- | ----------------------- | ---------------------------------------------------------------------- |
| Host Debug graph, Hermes, after the change (re-measured)                          | `ninja -t compdb`       | `-O2` on 490 of 490 Hermes compile lines in the two probe targets      |
| Host Debug graph, repo `vnext` sources and most of folly, glog, fmt (re-measured) | `ninja -t compdb`       | no `-O` flag, so `-O0` (45 of 45 repo lines, 350 of 371 other lines)   |
| QNX Hermes, `qnx805-build:/home/qnxbuild/hermes-qnx-full` (2026-11-13)            | `compile_commands.json` | `MinSizeRel`, 446 of 446 lines at `-Os`, no `HERMES_SLOW_DEBUG` define |
| QNX Qt 6.12.0 public, `qt-6.12.0-public/qnx-build` (2026-11-12)                   | `compile_commands.json` | 2341 lines `-O2`, 647 `-O3`, none without `-O`                         |
| QNX repo sources, `pr149b/qnx-build` (2026-11-19)                                 | `build.ninja` `FLAGS`   | `Release`, 701 of 701 lines `-O2`                                      |

The QNX rows (read in the earlier session, raw output lost) come from existing trees in the `qnx805-build` container, not
from a fresh QNX build. QNX Hermes is `-Os` and comes from
`scripts/configure-hermes-qnx.sh`, so this change does not touch it. The
`HERMES_SLOW_DEBUG:BOOL=ON` in that cache has no effect because the define only
applies to Debug configurations. The ADR-0017 `-O0` Qt problem is not present in
the 2026-11-12 tree.

## Not covered

- No QNX, Pi 400 or Marathon run.
- No Linux timing; the Linux CI legs of this PR give the first.
- The repo's own host targets stay at `-O0` in Debug. Changing that would affect
  the sanitizer lanes and is outside S1.
