# NEXT_ACTIONS.md

## PR 159 review follow-up — 2026-11-14

Publish the final integrated source/evidence tree and inspect its required hosted ON/OFF checks. Resolve the two host findings using the actual candidate/control/restoration evidence; retain the three artifact/device evidence requests. Run current-target image/input/teardown and release-app checks with host, image, source/artifact hashes, full commands and exit statuses. Do not infer target completion from host tests.

[Source, commands, hashes, limitations and rollback](../validation/pr159-review-followup-2026-11-14.md). Image results retain source 252b128; full-host results name b6bf7eb and a separate 55-file source inventory. Initial and restored host binaries share SHA-256 55af47f994614c2ca0110e3dd77004822b0c479772631c7afe4fbb2355712cd1. The consumer merge changes no tested source-tree identity. Prior 125-entry Release logs remain preserved with their stale-MOC omission; the corrected 126-entry run executes that disk-clear regression. Host ASan scope and uninstrumented dependencies are stated explicitly.

## PR 159 drawer route and pointer correction — 2026-11-13

The drawer now selects its navigator destination across all Home child tabs.
Native hover/focus decoration no longer obscures the row's input target.
Commit 1f809ce has failing old-source controls, passing JS/type/layout/bundle
checks, 34 QML cases, the native input regression and zero review findings.
Nine physical Pi captures show one selected row across five destinations and
all five Home tabs. The transplanted full host retains two separate failures;
exact-head CI, the clean combined build and owner-tile promotion remain open.
[Evidence, provenance and rollback](../validation/pr159-drawer-route-2026-11-13.md).

## PR 159 drawer selection correction — 2026-11-13

SVG d38097f is stacked into PR 159 with both source/evidence sets preserved.
Dependency/type/docs checks pass; the full local JS runner stops at its Linux-only
consumer-preview test on macOS. PR 161 renderer ON has two complex-corpus timeouts;
merge and clean combined target qualification remain pending.

The owner caught two teal rows after Components → Home. The renderer skipped
transparent background updates, leaving the old selection painted. Commit
cf43271 clears transparent/removed generic View fills; its regression fails
before the fix and all eight adapter cases pass with the isolated delta.
Pi captures now show one teal row for each route, including the final binary/
HBC pair. The independent review's interrupted-swipe issue is also fixed with
Zustand gesture state and three regressions. Final four-file review has zero
findings. Exact-head CI and clean combined qualification remain pending.
[Evidence and rollback](../validation/pr159-drawer-selection-2026-11-13.md).

## PR 159 drawer and module lifetime continuation — 2026-11-13

Committed d257b5a retains one TurboModule per successful host lookup; its forced
GC regression fails without retention and passes after restoration. The same
optimized device bundle now completes its image API sequence. Commit 874ea81
fixes JS drawer swipe reversal with Zustand state and native drawer dynamic
metadata/click transport. Pi captures show hover, keyboard focus and Enter
navigation; the final binary/HBC repeat navigation and image APIs. All 34
standalone QML cases pass in Release and ASan/UBSan. The transplanted Linux host
suite retains two separate failures, so only its lifetime case is qualified.
Inherited cross-build flags/object reuse are corrected in the receipts. CI found cross-test mock caching; the settlement case now runs in a child
process and the four router tests pass in shared-runner order. A fresh
combined build and exact-head CI/review remain required before final promotion.
[Sources, artifacts, limits and rollback](../validation/pr159-drawer-runtime-2026-11-13.md).

## PR 159 fetch worker continuation — 2026-11-13

File/data and disk-cache fetch I/O now run on bounded workers with owner-thread
completion. Fetchers share a network manager; corrupt/oversized cache recovery
and safe bounded redirects have regressions. Six Release native executables
and three ASan/UBSan image executables pass, as does the QNX cross-build and
Pi 400 resize/cache/ImageRef/blurhash check. The initial slice's independent
review has zero findings. Exact-head hosted checks remain required. The standalone image-cache link exposed a missing ImageDecode.cpp source;
that graph is corrected and its clean local loopback test passes. Explicit
cache API and ImageRef decode/hash/encode worker conversion remains open.
[Commands, evidence and rollback](../validation/pr159-image-io-2026-11-13.md).

## PR 159 image device continuation — 2026-11-13

The c2badc4 implementation head passed all hosted checks. On the custom-image
Pi 400, the new Zustand gallery driver shows a second image load after growth
from 20 to 240 pixels, successful cache/ImageRef/blurhash APIs, GIF frame
changes after start and stable pixels after stop. Window size remains 1280x800.
The loader I/O, cache recovery, redirects, navigation and wider conformance
work remains open. This driver needs its own CI and review.
[Evidence and rollback](../validation/pr159-device-image-2026-11-13.md).

## PR 159 image resolution continuation — 2026-11-13

Growing a downscaled still image now schedules an adequate decode, respecting
resource locks and decode options. All 30 native image-item host cases pass;
the growth and initial-decode race controls fail as expected. The review
finding is fixed; new-head CI and target
resize checks remain pending. Other image-loader/navigation gaps stay open.
[Receipt and rollback](../validation/pr159-image-resize-2026-11-13.md).

## PR 159 build repair — 2026-11-13

Inspect the pushed head's required renderer checks. Continue #161 before merging #159, then address the image-loader gaps in the November 13 handoff.
[Commands and limits](../validation/pr159-table-initializer-2026-11-13.md).

## PR 161 continuation (2026-11-14)

1. Inspect the published packaging follow-up's exact head and required checks.
   The preceding `13a0e49` head passes all nine workflows, both renderer lanes
   and all four corpus shards. Preserve that evidence and require the new
   head's checks at their existing limits.
   Retain any failed-job logs; do not infer the original timeout cause from
   the new nested-scene correctness results.
2. On the licensed QNX SDP 8.0.5 host, configure a fresh combined candidate
   using the pinned React Native/Hermes graph and selected public Qt runtime,
   with SVG enabled. Use two build jobs and no inherited object-reuse compiler
   launcher. Record the compiler/optimization flags, installed-header closure,
   QtSvg library/plugin closure and executable/bundle/runtime hashes, following
   the [corrected build receipt](../validation/pr161-device-gallery-2026-11-13.md).
3. Follow the [Pi 400 run/control procedure](HANDOFF_2026-11-13.md) for a single
   guarded SVG gallery instance. Exercise nested-child removal and two nested
   viewBoxes under group opacity, plus the existing per-element/attribute,
   image, ForeignObject input and interaction cases. Capture actual Screen
   output, logs, readback hashes and the exact board/runtime identity. Restore
   the owner's app afterward; keep boot, window and input policy unchanged.
4. Record target results and independent target review before marking the PR
   ready. Qualify the final combined PR 161/159/149 candidate separately;
   stock-QSTI and Marathon evidence remain separate from the historical custom
   Pi 400 result. All v1 gates remain pending.

Local completed checks and source hashes are in the
[2026-11-14 receipt](../validation/pr161-nested-roots-2026-11-14.md).

## PR 161 cross-build recipe correction — 2026-11-13

The native consumer used inherited Release/O1 flags with selected GCC warning
exceptions; public Qt libraries were O2. Its launcher reused upstream objects
without independently checking flags/header closure, while project sources
were recompiled. Device behavior and hashes stand as scoped evidence. Use a
fresh build without that launcher for final combined qualification.
[Corrected receipt](../validation/pr161-device-gallery-2026-11-13.md).

## PR 161 gallery and target continuation — 2026-11-13

Required checks passed at a52ffff. The current-source Pi run then exposed the
SvgCss root import; it now uses the pinned CSS subpackage and has a failing/
passing Metro regression. The twelve-card gallery renders on the real lean
VM/public QtSvg runtime, including bundled/SVG/HTTPS images and live
ForeignObject input blocking. Circle/group counters and rotation pause pass.
The added nested-invalidation control fails without outward forwarding; all
eight restored native host CTests pass. Prior review threads are resolved.
This follow-up still needs its exact-head checks and independent review.
[Artifacts, element table and rollback](../validation/pr161-device-gallery-2026-11-13.md).

## PR 161 corpus continuation — 2026-11-13

Inspect PR 161 exact-head renderer CI after publication. Verify all four corpus shards and retain their diff artifacts. Complete the handoff device checks before integrating #159 and #149.
[Receipt](../validation/pr161-corpus-shards-2026-11-13.md).

## PR 105 continuation — 2026-11-10

Review and publish the documentation follow-up to `c09a5b1`, inspect its
current-head CI, then resolve the mandatory-handoff thread. Run
`node tests/js/nitro-platform-codegen.test.mjs` and
`pnpm nitro:platform:check` after integrating another generator change.
Do not count PR 110's device smoke test as evidence for this independent
branch. [Receipt and rollback](../validation/pr105-merge-readiness-2026-11-10.md).

## Dependency-security follow-through (2026-10-03)

Review the braces depth backport and guarded audit in `docs/security/braces.md`.
Require the draft PR's exact head to pass SDK 58 validation before considering
merge. When upstream publishes a fixed release, replace the temporary patch
and attestation together and rerun all dependency and Metro checks. Do not
interpret the guarded pass as a clean raw registry audit.

## Bundled React probe follow-through (2026-10-04)

The shared probe and companion PR #28 (`24ba958`) passed 11/11 combined native
CTests. Once the companion changes are integrated, rerun the full host renderer
preset plus both probe CTests on the final combined commit before promoting it.
Keep this draft dependent on that host graph; its standalone main baseline is
not a passing native build. Follow [the shared probe runbook](../react-render-probe.md)
for later deployment of the binary **and** Metro bundle to QNX. Keep geometry,
pixel/on-glass, and physical-target results separate.

## 2026-10-04 image-loader implementation (host only)

Added real asynchronous image fetching/header handling, bounded decoded memory
prefetch/query and shared Qt Quick provider reuse. The exact Qt 6.11.1 Linux
loopback HTTP/cache/provider test passed locally; the QnxImageLoader translation
unit compiles against the pinned RN 0.88 headers. The existing Image.getSize and new Hermes Promise contract cases also pass
locally under ASan, using the full-host graph build repairs from draft PR #28.
The image adapter/backend/provider/factory and host allocation site were rebuilt
together for that targeted integration run; this is not a full-suite pass.
See `docs/image-loader.md` for commands, contract and limits. The dedicated
image-loader workflow runs real local HTTP tests, without external services.

QNX SDP/Pi execution and Qt Network size/build-time deltas remain pending.
The existing SDK58 audit finding is separate and tracked by draft PR #27;
this image change does not bypass or downgrade that audit gate.

## 1. Run the current physical Pi/QSTI validation lane

**Objective:** Prove the merged Screen/device path on the current target
baseline: QNX SDP 8.0.5 with QSTI 0.5.0 build 50 on Raspberry Pi 4 or
Raspberry Pi 5.

**What is already proven:** The older QNX SDP 8.0.2 line cross-built the
AArch64 Screen/C++20 targets, and a QNX x86_64 VM successfully ran both the
C++20 probe and `rnqnx_playground`. Those results are valuable historical
evidence, but they predate Expo SDK 58 / React Native 0.88 and do not close the
current physical-target gate.

**Current tooling:**

- `scripts/pi-preflight.sh` verifies the licensed host environment, AArch64
  compiler target, SSH, target architecture, `/dev/screen`, and writable
  deployment directory.
- `scripts/install-pi.sh` configures/builds the AArch64 target, deploys
  atomically, runs it, and captures stdout, slog2, and a Screen screenshot when
  available.
- Stock QSTI defaults are `qnxuser@qnxpi.local`; override only when the board
  differs.

**Procedure:**

1. Install/license QNX SDP 8.0.5 and the matching Pi 4 or Pi 5 QSTI package.
2. Flash QSTI 0.5.0 build 50 and boot the Pi with its display attached.
3. Source the SDP environment or set `QNX_SDP_ENV`.
4. From the repository root run:

   ```bash
   pnpm install --frozen-lockfile
   pnpm pi:preflight
   pnpm pi:install
   ```

5. The installer performs three launch/exit cycles by default, verifies every
   Screen initialization and clean shutdown, then writes a SHA-256 inventory to
   `artifacts/pi/<timestamp>/manifest.json`.
6. Preserve the generated evidence and record the exact SDP/QSTI/board identity
   in `TEST_STATUS.md`. Increase `RNQNX_PI_RUNS` for a longer cleanup/resource
   soak when useful.

**Acceptance criteria:**

- Target reports AArch64le and Screen is ready.
- The build uses the current licensed QNX 8.0.5 toolchain.
- `rnqnx_playground` initializes Screen and exits cleanly.
- stdout and slog2 evidence are captured; screenshot evidence is captured when
  the QNX utility is present.
- Repeated launches do not reveal retained Screen resources or startup drift.

**Current blocker:** Physical target execution has not yet been captured on the
current QSTI line. The repository tooling itself is ready.

## 2. Build and run the current Hermes QNX port on the Pi

**Objective:** Validate the already-merged RN 0.88 Hermes/QNX source rebase with
the licensed QNX SDP 8.0.5 toolchain and the physical AArch64le target.

**Already implemented:**

- React Native selects `hermes-v260318099.0.2`.
- The audited Hermes source commit is
  `4188b63f24026ca62b79dc81d95db122a59c6089`.
- `scripts/prepare-hermes-qnx.sh` prepares that exact source and applies the
  current QNX patcher rather than replaying the old RN 0.86 patch.
- `scripts/configure-hermes-qnx.sh` performs the QNX cross-configuration using
  the matching host `hermesc`.
- `cmake/dependencies/HermesQNX.cmake` imports the cross-built Hermes/JSI
  artifacts into the platform.
- Hermes QNX source-contract CI is green on the merged implementation.

**After the Screen lane passes:**

```bash
pnpm qnx:runtime:validate
```

The command prepares and cross-builds the exact Hermes source, enables the QNX
Hermes/Nitro/Expo targets, builds and deploys `rnqnx_runtime_qnx_probe` plus
`libhermesvmlean.so`, and runs JavaScript through Hermes that calls both
`QnxPlatformInfo` through `NitroModulesProxy` and
`expo.modules.ExpoQnxPlatform`. Preserve the resulting
`artifacts/pi-runtime/<timestamp>/` evidence.

**Dependency:** This does not block the native Screen bring-up. A full React
Native/Hermes/QNX claim still requires the combined runtime probe to execute on
the Pi and then the React/Fabric/Qt renderer to be moved onto that runtime.

## 3. Make the rendered app look and behave like an app

**Done:** a React Native bundle renders to Qt Quick items on the development
host. `@rn-qnx/react-native-qnx` supplies `Platform.js` and `BaseViewConfig.js`,
Metro selects them by resolved path, and the host provides microtasks, timers,
and platform constants.

**What remains, roughly in the order it will be missed:**

1. **`TextInput` JS wrapper, placeholder, multiline, focus/blur, submit, selection, and key-press events are done.** `packages/react-native-qnx/Libraries/Components/TextInput/TextInput.js` now routes `TextInput` to the iOS single-line descriptor on QNX, and the QML wrapper supports a placeholder overlay, `TextInput`/`TextEdit` switching, and the `focus`/`blur` imperative commands. Still missing: IME.
2. **Scroll gaps.** `scrollTo`, `scrollToEnd`, `onScroll`, `onScrollBeginDrag`, `onScrollEndDrag`, `onMomentumScrollBegin`, and `onMomentumScrollEnd` now reach the `Flickable`. Still missing: `contentContainerStyle`, `zoomScale` (hard-coded to 1), and full `velocity`/`targetContentOffset` fidelity.
3. **Smaller component gaps.** Bundler assets (`require('./x.png')`) and real `NativeAnimated` view updates (opacity/value back-propagation). `resizeMode`, `defaultSource`, `Image` load events, `ImageBackground`, `Image.getSize`/`prefetch`, `numberOfLines` ellipsis, `KeyboardAvoidingView`, `Button`, `TouchableOpacity`, multi-touch, pointer capture, and `ActivityIndicator`/`Switch`/`Modal`/`SafeAreaView`/`StatusBar`/`RefreshControl` are now covered — see `docs/component-support.md`.
4. **Nitro for app-facing modules.** ADR-0004 settled the boundary: React
   Native's own modules stay C++ TurboModules because that is what its
   JavaScript looks up, and Nitro is for the modules an app author writes. The
   merged QNX Nitro runtime now registers a synchronous `QnxPlatformInfo`
   HybridObject. Async callbacks/Promises remain intentionally unavailable until
   the QNX React/Qt runtime supplies the real JS/event-loop dispatcher.

**Acceptance criteria for the next slice:** a `TextInput` accepts typed
characters in the windowed playground and its `onChangeText` reaches
JavaScript, proven the way the touch and scroll paths were — by reading the
value back out of the runtime, with the assertion mutation-checked.

## 4. Target-validate the merged Nitro and Expo QNX extensions

The source-backed designs are now implemented as separate layers:

- **Nitro QNX:** `@rn-qnx/qnx-nitro-runtime` builds Nitro's audited shared C++
  sources on QNX, installs Nitro into JSI, and registers the synchronous
  `QnxPlatformInfo` HybridObject.
- **Expo Modules QNX:** `@rn-qnx/expo-modules-core-qnx` installs
  `globalThis.expo.modules` through a QNX JSI registry and ships an
  `ExpoQnxPlatform` proof module.
- **Expo QNX autolinking:** `@rn-qnx/expo-qnx-autolinking` reuses Expo's
  package discovery and adds the missing QNX resolver/CMake generation path.

Their source-contract and host/tooling checks are green. What remains is
physical QNX validation and integration with the full target runtime. Do not
treat these source implementations as proof that arbitrary Android/iOS Expo or
Nitro modules work on QNX: each native package still needs an explicit QNX
implementation/target, and Nitro async work still needs the actual QNX JS
dispatcher.

## PR #111 — executable continuation (2026-11-10)

1. Inspect the hosted `SDK 58 full host renderer` runs on
   `295f7240099c7371dc8988f4d411b1840888a415`. Confirm the ON lane executes native
   SVG, real React/Fabric SVG, motion, software graphics and null-RHI graphics,
   and the OFF lane contains none of those five tests.
2. If a lane fails, retain its exact CMake selections and JUnit/log artifacts,
   fix the reported failure and rerun that candidate; do not bypass inventory.
3. Follow [the v2 reproduction guide](../v2/validation.md) for the matching
   QNX build and target qualification. Keep QNX Screen and Marathon/Wayland
   results separate, and keep feature switches OFF until their required
   acceptance is recorded.

Do not mark Worklets, Reanimated, RNGH, full react-native-svg or GPU libraries
supported from these native primitive tests. The release record remains pending.
