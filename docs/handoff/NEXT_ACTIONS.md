# NEXT_ACTIONS.md

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
pnpm hermes:qnx:prepare
pnpm hermes:qnx:configure -- aarch64
cmake --build build/hermes-qnx-aarch64 --parallel --target hermesvmlean jsi
```

Then configure the platform with `RNQNX_BUILD_HERMES_QNX=ON`, build
`rnqnx_hermes_qnx_probe`, deploy it with the same target helpers, and execute
it on the Pi. Preserve compiler, ELF, stdout, and target logs as current-line
evidence.

**Dependency:** This does not block the native Screen bring-up. A full React
Native/Hermes/QNX claim still requires the current Hermes binary to execute on
the Pi and then be wired into the target React/Fabric/Qt runtime.

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
