# NEXT_ACTIONS.md

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
