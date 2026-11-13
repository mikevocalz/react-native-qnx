# V2 native Qt slice: validation receipt

Recorded on 2026-11-10 for `feat/v2-native-qt-apis`, based on
`0c91be9bbb4e7cd6dabccf8cdfe441c7fa57c629`. This receipt covers the commit that
contains it; the PR records the published head. The exact changed implementation
bytes are listed in [source-files.sha256](evidence/source-files.sha256).
[candidate.json](evidence/candidate.json) records dependency, bundle, compiler
and build identities. Documentation and evidence are excluded from the source
manifest to avoid a self-referential hash.

## Result

The first native Qt v2 slice passes its Linux host gates. React loads the new
SVG component through Metro and Hermes, mounts it through Fabric and Yoga,
renders visible pixels, and receives native events. Motion and graphics expose
native C++ contracts. All three capabilities default OFF.

The PR remains **draft** under the repository's `qnx-platform-validation` skill:
this candidate has no QNX cross-build, QNX device or Marathon compositor run.
Earlier main-branch Pi receipts are historical prerequisites, not results for
this candidate. No third-party graphics or animation port is marked supported.

## Candidate and environment

| Item                    | Identity / scope                                                                                                     |
| ----------------------- | -------------------------------------------------------------------------------------------------------------------- |
| OS / build              | Linux x86-64, Debug, CMake 4.4.4, Ninja                                                                              |
| Full renderer compiler  | Clang 19.1.7 with the system GNU C++ runtime; module scanning disabled because this project does not use C++ modules |
| Focused native compiler | GNU 14.2.0                                                                                                           |
| Qt                      | Prebuilt Qt 6.11.1; Core, Gui, Qml, Quick, Svg and Test; offscreen platform plugin                                   |
| Graphics lanes          | Qt software scene graph and explicitly selected null RHI; no hardware GPU claim                                      |
| Node / package manager  | Node 26.13.0, pnpm 12.8.1, frozen lockfile                                                                           |
| React / React Native    | 19.3.0 / 0.88.1, unchanged                                                                                           |
| Fabric generation       | Actual `@react-native/codegen` 0.88.1, 11 checked-in C++/header outputs                                              |
| Hermes                  | `hermes-v260318099.0.5`, source commit `9a115f15785851f4ff2747ebb090ddd066fc8ad2`, unchanged                         |
| Nitro                   | Core, modules and Nitrogen remain exactly `1.0.0-alpha.nightly.105.g69d365fc9b18`                                    |
| Target image / display  | None exercised; the tests use bounded offscreen host windows                                                         |

The full build uses `RNQNX_BUILD_HERMES=ON`,
`RNQNX_REQUIRE_FULL_HOST_RENDERER=ON`, `RNQNX_ENABLE_HOST_SANITIZERS=ON`, and
the three new `RNQNX_ENABLE_QT_{SVG,MOTION,GRAPHICS}` options. The same full
renderer regression targets were rebuilt and executed with those three options
OFF. An independent fresh default configuration also verified that the native
targets are absent when the options are omitted.

The extracted SDK environment needed explicit GLX/EGL include and library
paths, plus ICU 76.1 development headers matching the system runtime. Toolchain
paths and effective cache selections are in the candidate record. No Qt SDK,
system library, target image, boot configuration or renderer dependency source
was modified.

## Executed gates

| Gate and command                                                                       | Outcome                           | Evidence / practical limit                                                                                                                      |
| -------------------------------------------------------------------------------------- | --------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `pnpm install --frozen-lockfile`                                                       | Passed                            | The new explicit Babel runtime dependency resolves to the already-locked 7.29.7; the new component declares the existing React peer             |
| `pnpm validate:dependencies`, `pnpm validate:manifests`, `pnpm validate:architecture`  | Passed                            | Existing exact framework pins and platform boundaries preserved                                                                                 |
| `pnpm typecheck`                                                                       | Passed                            | Repository TypeScript checks                                                                                                                    |
| Explicit `tsc` on both new public declarations/spec                                    | Passed                            | Uses RN 0.88.1's exported `CodegenTypes`; command below                                                                                         |
| `pnpm qt:svg:check`                                                                    | Passed                            | Regeneration matches all 11 committed outputs                                                                                                   |
| Metro `build qt-svg-fixture.js --platform qnx --dev false`                             | Passed                            | Real QNX bundle and static Fabric view configuration; bundle hash in candidate record                                                           |
| `ctest --test-dir build/v2-clang --output-on-failure -R '^rnqnx_qt_'`                  | **5/5 passed**                    | [Summary](evidence/native-tests.txt), [test output](evidence/native-detail.txt); actual integrated SVG plus motion, SVG and both graphics lanes |
| Full existing renderer regression, v2 flags ON                                         | **4/4 passed**                    | [Result](evidence/renderer-enabled.txt); React host, mutation adapter, shadow tree mounting and Qt Quick mounting                               |
| Same full renderer regression, v2 flags OFF                                            | **4/4 passed**                    | [Result](evidence/renderer-disabled.txt); SVG registry presence/absence is asserted                                                             |
| Independent fresh Qt default-OFF build and CTest                                       | **4/4 passed**                    | [Result](evidence/default-off.txt); 1,000-item Qt probe, mounting, C++20 and fake Screen tests; no new targets                                  |
| Standalone motion with ASan/UBSan                                                      | **1/1 passed**                    | [Output](evidence/motion-sanitized.txt); actual item values, transformed coordinates, clock advancement and lifecycle races                     |
| Standalone SVG with ASan/UBSan                                                         | **1/1 passed, 55 QtTest rows**    | [Output](evidence/svg-sanitized.txt); visible rendering, source ownership, bounds and thread-affinity rejection                                 |
| Standalone graphics with ASan/UBSan                                                    | **2/2 passed**                    | [Output](evidence/graphics-sanitized.txt); 7 QtTest results in each software/null lane                                                          |
| Three `CMAKE_SYSTEM_NAME=Generic` configurations, one new flag ON at a time            | **3 expected rejections**         | [Output](evidence/unsupported-hosts.txt); fail before Qt discovery                                                                              |
| `pnpm test:cpp:host` without the additional Qt prefix                                  | **2/2 passed**                    | [Output](evidence/cpp-baseline.txt); C++20 and fake Screen host lane only                                                                       |
| `pnpm test:js`, followed by individual test-file execution to expose remaining results | **50/51 files passed; 1 blocked** | [File outcomes](evidence/js-files.json); the standard command exits nonzero at the blocked test                                                 |
| `pnpm format:check`, `git diff --check`                                                | Passed                            | Formatting and whitespace checks, not runtime evidence                                                                                          |

The blocked file is `marathon-development-launcher.test.mjs`. Its Wayland
fixture cannot create a Unix listening socket in this environment:
`listen EPERM`. The [actual failure](evidence/wayland-test-blocked.txt) is
retained; it is not counted as a pass or a Marathon run.

Sanitizer scope is deliberate. The focused standalone lanes instrument the
new native libraries and their tests with address and undefined-behavior
sanitizers, including vptr checks. Leak detection is disabled for these
prebuilt-Qt host runs. Qt itself is not instrumented. The full renderer build
uses the existing host sanitizer policy, and the new Fabric test executable
adds ASan; that is not whole-program instrumentation of Hermes, React Native,
Qt or every repository library.

The initial full GCC build hit existing upstream React Native warning failures
under `-Werror`. The successful full gate uses Clang. No broad warning
suppression or modification of `ReactNativeRenderer.cmake` is included.
Generated SVG translation units have narrow warning exceptions for upstream
generated code; the hand-written native code retains strict warnings.

The first published CI validation caught a missing parameter annotation in the
codegen script's whitespace-normalization helper (`TS7006`). A follow-up adds
the JSDoc type and passes `pnpm typecheck` and `pnpm qt:svg:check`; see the
[follow-up output](evidence/codegen-typing-fix.txt). This annotation changes no
generated C++ bytes or native runtime behavior. Hosted CI status remains
separate from the local gate results above.

### Hosted feature coverage follow-up

The initial hosted renderer matrix left all three Qt v2 switches OFF. Its
green results therefore covered the existing default renderer, not the new
native feature tests. The enabled-input lane now explicitly enables SVG,
motion and graphics and checks deterministic SVG binding generation. The
other lane explicitly keeps these switches OFF; the product defaults are
unchanged.

The full-renderer validator requires all five feature tests when their
corresponding switch is enabled: native SVG, real React/Fabric SVG, motion,
software graphics and null-RHI graphics. It also rejects those registrations
when the feature is OFF. A missing feature test can no longer be hidden by
successful baseline CTests. The existing evidence upload retains the actual
CTest inventory, JUnit results and CMake selections for each lane.

The orchestration regression reproduces the omission with the original
validator and passes with the corrected validator. It exercises each missing
feature test, independently enabled switches and accidental registration in
an OFF lane using fake executables; these are validator behavior checks, not
native runtime results. Repository typechecking passes. Actual new hosted
matrix execution must be evaluated on the follow-up commit after publication;
no QNX or Marathon device evidence is added by this change.

The follow-up applies the repository validation skill and Callstack's
`github-actions` skill at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`: keep
run-specific artifacts and test results attached to the exact candidate.

## Reproduce the focused full-host gate

Use the repository's host setup with Qt 6.11.1 and a complete native dependency
closure. These commands assume its compiler, GL and ICU prerequisites are
already available; the recorded candidate identifies this run's selections.

```sh
pnpm install --frozen-lockfile
pnpm qt:svg:check
pnpm exec tsc --ignoreConfig --noEmit --skipLibCheck \
  --target ES2022 --module NodeNext --moduleResolution NodeNext \
  packages/react-native-qnx/qt/QtSvgDocumentNativeComponent.ts \
  packages/react-native-qnx/qt/index.d.ts

cmake -S . -B build/v2-clang -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_PREFIX_PATH=/path/to/qt/6.11.1/gcc_64 \
  -DRNQNX_BUILD_HERMES=ON -DRNQNX_REQUIRE_FULL_HOST_RENDERER=ON \
  -DRNQNX_ENABLE_HOST_SANITIZERS=ON \
  -DRNQNX_ENABLE_QT_SVG=ON -DRNQNX_ENABLE_QT_MOTION=ON \
  -DRNQNX_ENABLE_QT_GRAPHICS=ON
cmake --build build/v2-clang --parallel 6 --target \
  rnqnx_qt_svg_fabric_test rnqnx_qt_motion_test \
  rnqnx_qt_svg_test rnqnx_qt_graphics_test
ctest --test-dir build/v2-clang --output-on-failure -R '^rnqnx_qt_'
```

The integrated test executes React and Hermes, rather than manually invoking
generated props or native event handlers. It verifies Yoga dimensions, initial
red pixels, changed blue pixels, load metadata delivered to JavaScript,
resize without duplicate load, malformed and oversized replacements, clear,
and actual React unmount releasing the native item. See
[`SvgFabricTest.cpp`](../../tests/native/qt-svg-fabric/SvgFabricTest.cpp) and
the [React fixture](../../apps/rn-playground/qt-svg-fixture.js).

For the existing full renderer regression, build
`qt_react_host_host_test`, `shadow_view_mutation_adapter_host_test`,
`shadow_tree_mounting_host_test`, and `qtquick_mounting_layer_host_test`, then
select those four exact names with CTest. Repeat after configuring all three
new flags OFF. Standalone sanitizer commands are documented in the individual
[motion](native-motion.md#run-the-native-checks), [SVG](svg-documents.md) and
[graphics](graphics-backends.md#evidence-and-rollback) guides and the effective
cache entries in the candidate record.

## Source, skills and review ownership

The architecture review used the current repository `AGENTS.md`, handoff,
component matrix, Marathon architecture/roster, platform-modernization skills,
Nitro v1 policy, and ADRs 0001/0002/0003/0004/0011/0013. Primary Qt, upstream
Worklets/Reanimated/Gesture Handler, Dawn, Skia and RN WebGPU sources are linked
in the [v2 design](README.md) and [graphics guide](graphics-backends.md).

| Applied source                                                   | Exact revision                                  | Decision applied                                                                                               |
| ---------------------------------------------------------------- | ----------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| Repository `qnx-native-api` and `qnx-platform-validation` skills | Base `0c91be9bbb4e7cd6dabccf8cdfe441c7fa57c629` | Preserve Fabric/Qt ownership; separate host and target evidence; keep incomplete target qualification in draft |
| Margelo `api-design`                                             | `1e9e17be6f41c838db472d6a4b943fc26355a3be`      | Public contracts, errors, ownership and examples before binding work                                           |
| Margelo `build-nitro-modules`                                    | Same pinned revision                            | Respect actual Nitro v1 generation; do not claim unimplemented service bindings                                |
| Margelo `cpp`                                                    | Same pinned revision                            | RAII, one owner, explicit thread affinity and safe reentrancy                                                  |
| Callstack `react-native-best-practices`                          | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`      | Test visible behavior and runtime flow; make no unmeasured FPS claim                                           |
| Callstack `writing-user-docs`                                    | Same pinned revision                            | Show actual application usage, input limits and recovery in the SVG guide                                      |

The Margelo sources were read in the table's order from pinned checkouts, not
installed globally. Exact upstream skill byte hashes are retained in the
existing [skills.lock.json](../platform-modernization/skills.lock.json).
Reading those sources does not establish tool installation or target support.

| Role                                         | Actual work / review                                                                                                   |
| -------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| Implementation DRI, primary agent            | Public React API, actual RN codegen, Fabric wiring, runtime fixture, full host build, integration and release evidence |
| Native motion owner, `qnx_repository_status` | Motion implementation; independent SVG ownership review; fresh default-OFF and unsupported-host checks                 |
| Native SVG owner, `animation_ports`          | Bounded SVG implementation and rendered-pixel tests; independent graphics teardown/recreation probes                   |
| Native graphics owner, `gpu_platform_design` | Graphics observer, source/Rust research; independent motion reentrancy review and SVG bridge review                    |
| Human merge / target qualification owner     | Unassigned; no approval or physical-device review claimed                                                              |

Peer review identified and resolved a motion endpoint reentrancy bug, QObject
affinity-change handling, and an unbounded SVG prop copy. The reviewed final
behavior has per-operation clocks, permanent detachment on affinity changes,
and size-before-copy SVG ownership. Independent graphics probes also exercised
100 rapid surface teardown/recreation cycles in both host lanes. Those scratch
probes supplement the committed regression tests; they are not a device claim.

## Remaining qualification, licensing and rollback

QNX cross-build, QNX VM, physical Pi 400/QSTI, Linux Wayland and actual Marathon
execution are **not run** for this slice. Hardware rendering, input-to-present
latency, idle wakeups, memory growth and frame-percentile budgets are also not
measured. No new performance budget or refresh-rate guarantee is asserted.

The SVG API accepts a bounded inline static subset and rejects external
resources, active content and unsupported markup before rendering. It does not
implement full `react-native-svg`. Motion and graphics remain native C++ APIs;
there is no installed Nitro motion/graphics JS API, worklet scheduler, gesture
registry, Dawn device, Rust renderer adapter or third-party package port in
this commit. Their acceptance gates remain open in the [v2 plan](README.md).

Qt SVG adds an optional Qt module to the consuming build and retains that
module's LGPL/GPL/commercial distribution obligations under
[ADR 0013](../adr/0013-open-source-qt.md). No Qt Quick 3D dependency is added;
that future optional module has its separate GPLv3/commercial terms. No premium
Forge source is copied, changed or made a platform prerequisite. The inspected
Forge engine is input to the adapter design, not proof of a GPU backend.

Rollback: leave the three new switches OFF and remove experimental component
imports. Reverting the slice removes its additive package entry and generated
files; no persistent application format or standard RN API is changed. The
next smallest runtime slice is explicit RN-JS/UI ownership plus an actual
Worklets bootstrap and lifecycle test. The graphics track first needs a pinned
Dawn build and a real QNX draw/readback/presentation test before adding the
shared-device Rust adapter.

## react-native-svg

Stage 4: `react-native-svg` 15.16.0 elements on Qt Quick, behind
`RNQNX_ENABLE_QT_RNSVG` (default OFF). Guide:
[react-native-svg.md](react-native-svg.md).

| Check                                                                           | Result                                                                                                                                   |
| ------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `pnpm qt:rnsvg:check`                                                           | Generated props and events match the package's specs (codegen 0.88.1, package 15.16.0)                                                   |
| `rnqnx_qt_rnsvg_test` (25 cases, golden pixels and hit tests)                   | 25/25 passed on a Linux aarch64 VM: Qt 6.12.0 offscreen software, GCC 13.3, ASan+UBSan, `-Werror` ([output](evidence/rnsvg-host-vm.txt)) |
| `rnqnx_qt_rnsvg_fabric_test` (unmodified package through Metro, Hermes, Fabric) | Runs in the `host-renderer` workflow, input-lifecycle ON leg; see the PR for the run                                                     |
| QNX aarch64 cross-build of the consumer app with the switch ON                  | See "Pi 400 device" below                                                                                                                |

### Pi 400 device

Run on 2026-11-13, one board: Raspberry Pi 400, QNX SDP 8.0.5 custom image,
public-source Qt 6.12.0 `-O2` runtime, OpenGL ES through the patched `libqqnx`
(the Qt Quick scene graph on the GPU; the SVG raster itself is CPU). The app is
`apps/component-gallery` built in the `qnx805-build` container with
`RNQNX_ENABLE_QT_RNSVG=ON` and `extra.rnqnx.componentName = RNQnxSvgGallery`
(the SVG cards without the tab shell), app id `org.rnqnx.svgshowcase`, own
directory, own launcher. Build flags matched the other gallery builds (`-O1`
C++, input lifecycle and window chrome ON).

| Check                                                                  | Result                                                                                                                                                                          |
| ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| App `--screenshot` (the app's own window)                              | Cards drawn: [paths, gradients, clip/mask/use](evidence/rnsvg-pi400-cards-1.png), [text](evidence/rnsvg-pi400-text-card.png), [touch card](evidence/rnsvg-pi400-touch-card.png) |
| Prop updates without remount                                           | The group's rotation (JS timer) and an `Animated.Value` radius change every frame; the rotation read 66 deg and the yellow circle was drawn in the second capture               |
| Repaint cost (`RNQNX_RNSVG_TRACE=1`, `QQuickPaintedItem` Image target) | Touch card (300x150, 6 elements, two props changing continuously): 1.97 to 2.34 ms per repaint over 840 repaints in about 35 s. First paints of 1012x150 cards: 1.7 to 13 ms    |
| JavaScript errors                                                      | None (`js fatal` count 0 in the live log)                                                                                                                                       |
| Per-element `onPress` on the device                                    | **Not exercised.** No physical or injected pointer was sent to the board's SVG window; the touch path is covered by the host Fabric test only                                   |
| Reanimated / Worklets                                                  | Not available on main at the time (stages 2 and 3 unmerged); props are JavaScript-driven                                                                                        |

Not measured: the `FramebufferObject` paint target (`RNQNX_RNSVG_TARGET=fbo`),
GPU memory, long-run frame pacing, and any other board. Getting the build onto
the Pi needed the public `react-native` entry to export `processColor`,
`PanResponder`, `Easing`, `TurboModuleRegistry` and `codegenNativeComponent`,
which react-native-svg imports; they are real implementations from React Native.
