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

5. Preserve the generated `artifacts/pi/<timestamp>/` evidence and record the
   exact SDP/QSTI/board identity in `TEST_STATUS.md`.
6. Repeat the run enough times to catch Screen cleanup/resource regressions.

**Acceptance criteria:**

- Target reports AArch64le and Screen is ready.
- The build uses the current licensed QNX 8.0.5 toolchain.
- `rnqnx_playground` initializes Screen and exits cleanly.
- stdout and slog2 evidence are captured; screenshot evidence is captured when
  the QNX utility is present.
- Repeated launches do not reveal retained Screen resources or startup drift.

**Current blocker:** Physical target execution has not yet been captured on the
current QSTI line. The repository tooling itself is ready.

## 2. Rebase Hermes QNX work onto the RN 0.88 source line

**Objective:** Turn the historical Hermes/QNX investigation into a current,
source-backed port for Expo SDK 58 / React Native 0.88.

**Current installed evidence:**

- `node_modules/react-native/sdks/.hermesv1version` →
  `hermes-v260318099.0.2`
- React Native resolves `hermes-compiler` 260318099.0.4.
- `node_modules/react-native/ReactCommon/react/runtime/hermes/HermesInstance.h`
  still provides the runtime seam checked by `validate:dependencies`.
- `patches/hermes-qnx-250829.patch` belongs to the older
  `hermes-v250829098.0.16` source and must not be applied blindly.

**Procedure:** Acquire/check out the exact current Hermes source, replay each QNX
change deliberately, and re-audit OS, threads, mmap, signals, stack handling,
and host-`hermesc` assumptions. Build/test first on the QNX VM where useful,
then on AArch64/QSTI.

**Dependency:** The native Screen Pi lane can proceed independently. A truthful
full React Native/Hermes/QNX claim depends on both the current Hermes rebase and
current QNX target validation.

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
   JavaScript looks up, and Nitro is for the modules an app author writes. No
   Nitro `HybridObject` is registered yet.

**Acceptance criteria for the next slice:** a `TextInput` accepts typed
characters in the windowed playground and its `onChangeText` reaches
JavaScript, proven the way the touch and scroll paths were — by reading the
value back out of the runtime, with the assertion mutation-checked.

## 4. Design Nitro, Expo Modules, and Metro QNX extensions separately

Installed source establishes the following contracts and gaps:

- Nitro C++: `margelo::nitro::install` and
  `HybridObjectRegistry::registerHybridObjectConstructor` exist, but Nitrogen's
  generated platforms are iOS/Android only.
- Expo Modules Core: `ensureNativeModulesAreInstalled` expects the
  `ExpoModulesCore` TurboModule and `globalThis.expo`; installed native installers
  are Apple/Android only.
- Expo Metro: `getDefaultConfig` is available through `expo/metro-config`, but
  its default platforms omit `qnx`.

Treat these as three explicit source-backed designs, not one guessed platform
shim. Each native file created on macOS must receive its own QNX validation queue
entry.
