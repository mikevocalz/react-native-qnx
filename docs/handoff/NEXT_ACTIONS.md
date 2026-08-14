# NEXT_ACTIONS.md

## 1. Complete QNX-RN-008 on the licensed build host and target

**Objective:** Compile the current Screen-only executable against the installed
QNX SDP 8.0.5, then run it on a real target. This is the only remaining
Milestone 0 exit gate.

**Files:**

- `cmake/toolchains/qnx-8.0.cmake`
- `CMakeLists.txt`
- `native/platform/qnx/application/main.cpp`
- `native/platform/qnx/screen/QnxScreenSurface.{h,cpp}`
- `tests/cpp/qnx-screen-fake/screen/screen.h`
- `scripts/{doctor,configure-qnx,build-qnx,deploy-qnx,run-qnx}.sh`

**Procedure:**

1. On x86-64 Ubuntu 24.04, install and license QNX SDP 8.0.5 plus the target
   BSP/Screen development files. Source its environment script.
2. Before changing native code, inspect the installed `screen/screen.h` and cite
   the declarations/constants used by `QnxScreenSurface`: `screen_create_context`,
   `screen_create_window_type`, `screen_set_window_property_iv`,
   `screen_create_window_buffers`, `screen_get_window_property_iv`,
   `screen_get_window_property_pv`, `screen_fill`, `screen_post_window`,
   `screen_destroy_window`, `screen_destroy_context`, and their referenced
   `SCREEN_*` constants. Reconcile the test-only header from that installed file.
3. Run:

   ```bash
   pnpm install --frozen-lockfile
   ./scripts/doctor.sh
   ./scripts/configure-qnx.sh aarch64 Debug
   ./scripts/build-qnx.sh aarch64 Debug
   ```

4. With `QNX_TARGET_IP` set, deploy and run:

   ```bash
   ./scripts/deploy-qnx.sh aarch64 rnqnx_playground Debug
   ./scripts/run-qnx.sh aarch64 rnqnx_playground
   ```

5. Run the executable repeatedly and use target-appropriate Screen/resource
   diagnostics to check cleanup. Record exact commands and output in
   `TEST_STATUS.md`; advance the two QNX-GFX queue entries only with evidence.

**Acceptance criteria:**

- The installed header/source audit finds every used Screen symbol or work stops
  with the exact mismatch recorded; no substitute API is invented.
- `qnx_platform` and `rnqnx_playground` compile and link with warnings as errors.
- The target shows the 1280×720 window alternating blue and black for up to 600
  frames, and exits cleanly.
- Repeated runs show no retained Screen context, window, or buffers.

**Current blocker:** This macOS workspace has no licensed SDP or target. See
`BLOCKERS.md`.

## 2. Produce the exact-tag Hermes QNX feasibility record

**Objective:** Turn the current Hermes identity into a source-backed QNX build
plan without assuming Apple/Android build seams are portable.

**Installed evidence:**

- `node_modules/react-native/sdks/.hermesv1version` →
  `hermes-v250829098.0.16`
- `node_modules/react-native/ReactCommon/react/runtime/hermes/HermesInstance.h`
  → `HermesInstance::createJSRuntime`
- `node_modules/react-native/ReactCommon/react/runtime/hermes/HermesInstance.cpp`
  → `hermes::makeHermesRuntime`
- `node_modules/react-native/ReactAndroid/hermes-engine/build.gradle.kts` and
  `node_modules/react-native/sdks/hermes-engine/hermes-utils.rb` → platform
  precedents only

**Procedure:** Acquire the exact upstream Hermes tag separately, map its OS,
thread, ICU, mmap, signal, and host-`hermesc` requirements to the installed QNX
toolchain, and write an ADR/investigation note. Do not add a QNX CMake adapter
until every called source symbol exists in that checkout.

**Dependency:** QNX-RN-008, because feasibility must be tested with the actual
compiler and sysroot.

## 3. Load a real bundle

**Done:** the runtime and renderer are connected. `QtReactHost` builds React
Native's `Scheduler` on a Hermes runtime with our `EventBeat`, component
registry factory, and `SchedulerDelegate`; surface commits mount onto Qt Quick
items with Yoga's geometry. Three of the five previously missing pieces are
implemented; the other two are avoided rather than faked, and both come back
with a real bundle.

**What remains:**

1. **`bindHasComponentProvider`**
   (`componentregistry/native/NativeComponentRegistryBinding.h:26`). JS view
   configs ask whether a native component exists; without it `View` will not
   resolve from JavaScript. The predicate should normalise through
   `componentNameByReactViewName` before answering.
2. **A JavaScript entry point.** `AppRegistryBinding::startSurface` throws
   unless the bundle installed `global.RN$AppRegistry`
   (`AppRegistryBinding.cpp:30-32`), and `stopSurface` needs
   `global.RN$stopSurface`. The host currently installs a no-op for the latter
   so empty surfaces can stop; a real bundle replaces it. Start with a minimal
   entry that calls `AppRegistry.registerComponent` directly rather than
   importing all of `react-native`, which drags in TurboModule expectations.
3. **`MessageQueueThread` and `PlatformTimerRegistry`**, if and when
   `ReactInstance` is adopted. The host drives the runtime directly today, which
   is why they are not needed yet — a real JS thread changes that.
4. **Metro.** A bundle has to be produced before it can be loaded:
   `metro-config-qnx` with `qnx` in `resolver.platforms`, and `bundle-qnx` in
   the CLI. Neither needs a device.
5. **Props past layout.** Backgrounds, borders, opacity, and transforms, each
   recorded in `docs/component-support.md`.

**Acceptance criteria:** a React bundle renders a `<View>` tree into a Qt Quick
window on the development host, with existing host cases green. No QNX toolchain
or device required, and no result from it promotes a QNX validation row.

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
