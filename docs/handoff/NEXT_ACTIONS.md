# NEXT_ACTIONS.md

## 2026-11-10 — Publish the PR #108 SDK restack

Publish the reviewed merge with parents #108 `66ac3b9` and #110 `d5d8190`, then
inspect that exact head's SDK and native CI. Retain the unchanged source-bound
native results and artifacts; no local native rerun is needed for this
documentation-only reconciliation. Earlier continuation entries below are
historical; existing target acceptance and all seven release gates stay open.
[Integration receipt](../validation/pr108-integration-2026-11-10.md).

## 2026-11-10 — PR #108 stacked SDK continuation

Publish the trigger-only follow-up on `3fa112e`, then require SDK 58 validation
to run and pass on #108's #110 base at the new exact head. Inspect that result
alongside the native jobs. Existing 47/47 ON / 25/25 OFF source-bound native
evidence remains intact; do not rerun unrelated native work for this filter edit.
[Integration receipt](../validation/pr108-integration-2026-11-10.md).

## 2026-11-10 — PR #108 integrated continuation

1. Publish the validated integration as a merge preserving #108 `47cd9d8` and
   #110 `73f7da0`. Native results are complete: 47/47 ON, 25/25 OFF and the
   intended DevSession negative-control assertion.
2. Verify the final published head's actual Actions jobs. Keep the
   [integration receipt](../validation/pr108-integration-2026-11-10.md), raw logs,
   source/artifact hashes and leak-detection limitation attached. Regenerate
   Metro fixtures explicitly after indirect platform-JS changes until CMake's
   dependency coverage is expanded.
3. Preserve the #104/#106/#110 dependency history and review the final PR base
   before merging. Human review remains separate from source automation.
4. Run the integrated candidate's named QNX and Marathon target scenarios and
   a real host Metro edit/reload workflow. Keep missing gates pending.

## 2026-11-10 — PR #110 SDK follow-through

Publish the filter-only follow-up to `73f7da0` and inspect its own SDK run.
Merge that published follow-up into dependent #108 to keep ancestry intact;
native sources and recorded evidence remain unchanged.
[Receipt](../validation/pr110-merge-readiness-2026-11-10.md).

## 2026-11-10 — Combined PR #106/#110 continuation

Review and publish the merge of #110 `2657620` and #106 `5e3bb88`, preserving
both parent histories. Inspect full native ON/OFF and public-import CI at the
published merge head. Keep #104 and #106 before their dependent #110 and
inspect the final target/diff. The Switch fixture integration described in
the older #106 entry is now completed and locally checked; target evidence
and actual Fast Refresh remain separate work.
[Combined receipt](../validation/pr110-merge-readiness-2026-11-10.md).

## 2026-11-10 — PR #110 integration follow-up

Publish the integrated #104/#110 candidate and inspect both full host-renderer
matrix jobs at that exact head, including required registrations for Modal
focus, controlled Switch, development runtime and frame tracing. Keep #104
before #110 and inspect the resulting base/diff. Run an actual host Fast
Refresh edit/reload/reconnect session separately; obtain exact artifacts,
flags and named interactions for future QNX acceptance. Preserve the positive
owner report and all seven pending release gates.

Receipt:
[pr110-merge-readiness-2026-11-10.md](../validation/pr110-merge-readiness-2026-11-10.md).

## 2026-11-10 — PR #104 readiness follow-up

Attach the tested artifact identities and actions to the positive owner feedback. Preserve existing #104 history; review #104 before the dependent #110 and verify the latter’s base/diff after retargeting. Keep ordinary gallery interaction, optional desktop behavior, and release qualification separate.

Receipt: [gallery-owner-report-2026-11-10.md](../evidence/gallery-owner-report-2026-11-10.md).

## PR 106 continuation — 2026-11-10

Review/publish this follow-up and inspect its CI before resolving the handoff
thread. When integrating PR 110, retain the style-array repair and move Switch
style/prop checks into its retained-hook test harness; preserve all Image tests.
Run `node tests/js/qnx-public-entry.test.mjs`,
`node tests/js/qnx-public-entry-metro.test.mjs` and the combined controlled-Switch
test/native-host checks. [Receipt and rollback](../validation/pr106-merge-readiness-2026-11-10.md).

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
