# NEXT_ACTIONS.md

## 1. Complete QNX-RN-008 on the licensed build host and target

**Objective:** Compile the current Screen-only executable against the installed
QNX SDP 8.0.2, then run it on a real target. This is the only remaining
Milestone 0 exit gate.

**Files:**

- `cmake/toolchains/qnx-8.0.cmake`
- `CMakeLists.txt`
- `native/platform/qnx/application/main.cpp`
- `native/platform/qnx/screen/QnxScreenSurface.{h,cpp}`
- `tests/cpp/qnx-screen-fake/screen/screen.h`
- `scripts/{doctor,configure-qnx,build-qnx,deploy-qnx,run-qnx}.sh`

**Procedure:**

1. On x86-64 Ubuntu 24.04, install and license QNX SDP 8.0.2 plus the target
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

## 3. Implement the TurboModules the bundle requires

**Done:** Metro bundles for `qnx` through `@rn-qnx/metro-config-qnx`, the
playground app produces a real bundle during the build, and the host installs
the bridgeless globals (`RN$Bridgeless`, `__fbBatchedBridgeConfig`,
`RN$registerCallableModule`) that React Native's JavaScript branches on.

**The blocker is precise.** Evaluating the bundle reaches
`TurboModuleRegistry.getEnforcing('ExceptionsManager')` and throws. The
playground bundle demands nine modules:

| Module                 | Note                                                     |
| ---------------------- | -------------------------------------------------------- |
| ExceptionsManager      | first to throw; needs to surface JS errors to the host   |
| SourceCode             | bundle URL for stack symbolication                       |
| DeviceInfo             | screen metrics and scale                                 |
| UIManager              | legacy shim; check what the Fabric path actually needs   |
| NativeMicrotasksCxx    | already a C++ module in React Native — check reusability |
| NativeIdleCallbacksCxx | same                                                     |
| FileReaderModule       | pulled in by a transitive import; may be avoidable       |
| SegmentFetcher         | same                                                     |
| WebSocketModule        | same                                                     |

**Procedure:**

1. Check which of these React Native already implements as generic C++
   TurboModules (`ReactCommon/react/nativemodule/**`) rather than per platform.
   Anything reusable is a link-list change, not new code.
2. Trim the entry point. FileReaderModule, SegmentFetcher, and WebSocketModule
   look like transitive imports rather than genuine needs; a narrower entry may
   drop them.
3. Implement the rest as Nitro HybridObjects per ADR-0004, which is the module
   architecture this project committed to. Do not add TurboModules by hand
   beyond what React Native's own generic C++ provides.
4. Install a `TurboModuleBinding` so `TurboModuleRegistry` resolves.

**Acceptance criteria:** the playground bundle evaluates, its component renders
through React, and the resulting Qt Quick items match the tree. No QNX toolchain
or device required.

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
