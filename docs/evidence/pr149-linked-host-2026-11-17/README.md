# Linked Worklets, Reanimated and Gesture Handler — Linux receipt

Date: 2026-11-17. Base: `af9dd3d3e449eee193763c20d5aa82f2c46d995e`.
DRI: PR #149 native-runtime workstream. Independent reviewer: SVG workstream
agent, with additional root review of integration and incremental bundling.

## Change and observed behavior

`rnqnx_playground_host` and `rnqnx_playground_qnx` now link each enabled runtime
extension. Their existing bootstrap code was conditional on definitions exported
by those libraries, so compiling the archives alone did not establish that the
application included the bootstrap. The consumer application's existing linkage
is unchanged. All three switches still default to OFF, and package pins are
unchanged.

The new opt-in host executable links the actual Worklets 0.13.1, Reanimated 4.7.1
and Gesture Handler 3.4.0 native targets with QtReactHost, Hermes and Fabric. Metro
bundles the real packages and public JavaScript imports. The fixture uses the
normal Qt mounting and input paths, with no replacement native module or runtime.
Its bundle target always invokes Metro so a transitive platform overlay change
cannot silently reuse an old fixture bundle.

The positive test passed these checks on two fresh host instances:

- A three-callback `requestAnimationFrame` chain reports monotonic timestamps and
  `ui-worklet`, then returns its result through `scheduleOnRN`.
- `withTiming(60)` completes and moves a red Fabric view from scene origin
  `(20, 20)` to `(80, 20)`. The old location is white and the new location red.
- Real Qt mouse press/move/release events reach an upstream `GestureDetector`.
  Its pan worklet moves a blue Fabric view from `(20, 100)` to `(60, 100)`, and
  the update and completion callbacks report translation X = 40. Old/new pixel
  checks agree with the exact scene origins.
- Stopping the surface runs the React cleanup and leaves no mounted root items.

A third fresh host admits a real library frame callback on the UI thread, waits
for a FIFO UI barrier without pumping GUI frames, and reads a shared value of
99 before destruction. The callback would change that value to 100; zero would
mean it had not been admitted. This proves work remains pending at teardown
without assuming one platform frame request per library callback. Processing Qt
events after destruction completed without an AddressSanitizer diagnostic.

The missing-bootstrap test loads the same real Metro bundle without native
extension installation. The host records the installed Worklets 0.13.1 error
`Cannot read property 'loadUnpackersWithCode' of undefined`, and its native proxy
is absent. The oracle checks the first diagnostic line and the absent proxy;
an unrelated throw with the same bundle filename is explicitly rejected. The
fixture inspects both direct evaluation errors and React Native's reported
JavaScript errors, because Metro can report a fatal import error while returning
normally from bundle evaluation.

## Commands and results

Environment: Linux x86-64, Node 26.13.1, pnpm 12.18.0, CMake 3.31.10,
Ninja 1.13.2, Clang 19.1.7, Qt 6.11.1, software/offscreen rendering. The frozen
workspace dependency installation succeeded. Hermes was built at the configured
revision `9a115f15785851f4ff2747ebb090ddd066fc8ad2`.

```sh
cmake --preset linux-host-renderer \
  -DRNQNX_ENABLE_INPUT_LIFECYCLE=ON \
  -DRNQNX_ENABLE_TEXT_INPUT_CONTRACT=ON \
  -DRNQNX_ENABLE_ACCESSIBILITY_BRIDGE=ON \
  -DRNQNX_ENABLE_WINDOW_CHROME=ON \
  -DRNQNX_ENABLE_QT_SVG=ON -DRNQNX_ENABLE_QT_RNSVG=ON \
  -DRNQNX_ENABLE_QT_NATIVE_UI=ON -DRNQNX_ENABLE_QT_EXPO_IMAGE=ON \
  -DRNQNX_ENABLE_WORKLETS=ON -DRNQNX_ENABLE_REANIMATED=ON \
  -DRNQNX_ENABLE_GESTURE_HANDLER=ON -DRNQNX_ENABLE_QT_MOTION=ON \
  -DRNQNX_ENABLE_QT_GRAPHICS=ON -DRNQNX_ENABLE_QT_VIDEO=ON
cmake --build --preset linux-host-renderer \
  --target rnqnx_worklets_integration_host_test rnqnx_playground_host --parallel 3
ctest --test-dir build/linux-host-renderer \
  -R 'rnqnx_worklets_(integration_host_test|rejects_missing_bootstrap)' \
  --output-on-failure
```

Configure and both linked executable builds passed. The final focused CTest run
passed 2/2 tests: positive 27.39 seconds, missing-bootstrap 3.19 seconds, total
30.60 seconds. [Raw CTest output](ctest.txt) records that run. The configured
inventory contains 59 tests; this receipt does not claim that all 59 were built
or executed. The generated images are below, with the second host's images also
retained in this directory.

| Initial state                                | Timing complete                           | Pan complete                         |
| -------------------------------------------- | ----------------------------------------- | ------------------------------------ |
| ![Initial red and blue views](0-initial.png) | ![Red view moved by timing](0-timing.png) | ![Blue view moved by pan](0-pan.png) |

The standalone Worklets lifecycle target was also configured with
`RNQNX_WORKLETS_SANITIZERS=ON`, built and passed its 1/1 CTest. The separate
[gesture cancellation receipt](../pr149-cancellation-2026-11-17/README.md)
records the 24-case native registry suite and before/after regressions.

Repository format, typecheck, dependency, architecture and documentation gates
passed. Of 86 JavaScript test files, 85 passed; the remaining
`marathon-development-launcher.test.mjs` is blocked by the execution environment's
`AF_UNIX` listen `EPERM`. The runner stopped there, and the remaining 67 files
were then executed individually and passed. The complete `pnpm test:js` command
is therefore not claimed green. The host inventory contract test covers all
eight combinations of the three extension switches and requires the integration
tests only when all three are ON.

A separate actual configure with the three switches OFF passed. Its CTest
inventory contains 15 tests and no integration tests, and its playground link
command contains none of the three extension archives or enabled definitions.
That is configuration and inventory evidence, not an OFF executable run.

## Review, sanitizer scope and limits

Independent review identified an overly broad negative-error substring check and
a teardown check that did not establish admitted work. They were replaced with
the first-line/proxy negative oracle and explicit pending shared-value proof.
The first real linked run then exposed Metro's separate reported-error channel
and frame-request coalescing; the fixture was corrected to observe those actual
contracts. The revised source received independent review with no remaining
scoped blocker before the final passing native run.

AddressSanitizer instruments the new executable target. This does not mean every
upstream React Native, Worklets, Hermes or Qt object is instrumented. The
standalone registry and lifecycle targets instrument their tested native sources
with ASan and UBSan. Leak detection is disabled because this environment uses
ptrace supervision; no leak claim is made.

The repository native API and platform validation skills, Margelo API design,
Nitro and C++ skills at `1e9e17be6f41c838db472d6a4b943fc26355a3be`, and Callstack
React Native guidance at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` informed the
contract, lifecycle and evidence boundaries. Source snapshots were read; no
personal skill installation or fresh CodeRabbit service review is claimed.
ADR-0020's explicit upstream bootstrap exception remains unchanged.

No QNX cross-build, physical target, presentation performance, busy-JS rendering,
dedicated React JS thread, additional recognizer, layout animation or scroll
qualification is established here. The PR remains draft pending its required
target and broader runtime evidence. Revert the linked-host test/linkage commit
to roll back this slice, or leave the three optional switches OFF.
