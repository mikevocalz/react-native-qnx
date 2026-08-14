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

## 3. Wire the runtime to the renderer

**Done:** Hermes builds and runs on the host, and the mounting path is proven
end to end from a hand-committed `ShadowTree`. CTest is 7/7 by default and 8/8
with Hermes.

**What remains** is the wiring between them. The audit below is against the
installed package; everything from `Scheduler` downward is generic C++ and needs
no porting. Five pieces have no generic implementation and are ours to write:

1. **`EventBeat` subclass** (`react/renderer/core/EventBeat.h:62`, `request()`
   at :90). Android drives it from Choreographer, Apple from a `CFRunLoop`
   observer; ours drives `induce()` from the Qt frame tick.
2. **`ComponentRegistryFactory`**
   (`componentregistry/ComponentDescriptorFactory.h:25-27`).
   `getDefaultComponentRegistryFactory()` is declared at :29 and **never
   defined anywhere in the package** — linking against it fails. Register
   `RootComponentDescriptor` and `ViewComponentDescriptor`, and consider
   `UnimplementedViewComponentDescriptor` as the fallback so an unknown
   component is a soft failure rather than a thrown `std::invalid_argument`
   (`ComponentDescriptorRegistry.cpp:112`).
3. **`SchedulerDelegate` subclass** (`scheduler/SchedulerDelegate.h:21`), all 13
   pure virtuals. Mount from `schedulerShouldRenderTransactions` (:38), not
   `schedulerDidFinishTransaction` (:28) — the latter fires synchronously on the
   commit thread and Apple makes it a no-op.
4. **`bindHasComponentProvider` predicate**
   (`componentregistry/native/NativeComponentRegistryBinding.h:26`), or JS view
   config lookup will not find `View`.
5. **`MessageQueueThread` and `PlatformTimerRegistry`**, required by
   `ReactInstance`.

**Ordered sequence:** construct `RuntimeScheduler`, install
`RuntimeSchedulerBinding`, put the scheduler into the `ContextContainer` under
`RuntimeSchedulerKey` (`Scheduler.cpp:76-78` asserts on it), fill a
`SchedulerToolbox` (`SchedulerToolbox.h:28-68`), construct `Scheduler` — which
builds the `UIManager` and installs `UIManagerBinding` itself
(`Scheduler.cpp:136-138`) — then evaluate the bundle, then start a surface.
`SurfaceManager` (`scheduler/SurfaceManager.{h,cpp}`) collapses registration,
layout constraint, and start into one call; it is complete, generic, and
currently has zero C++ callers in the tree.

**Two traps recorded from the audit:** `constraintLayout` must run before
`start()` because `LayoutConstraints::layoutDirection` defaults to `Undefined`
and `SurfaceHandler.cpp:43` asserts on it; and `AppRegistryBinding::startSurface`
throws unless the bundle has installed `global.RN$AppRegistry`
(`AppRegistryBinding.cpp:30-32`), so a first bring-up may want a minimal JS entry
that calls `AppRegistry.registerComponent` directly rather than importing all of
`react-native` and its TurboModule expectations.

**Acceptance criteria:** a bundle renders a `<View>` tree into a Qt Quick window
on the development host, with existing host cases green. No QNX toolchain or
device required, and no result from it promotes a QNX validation row.

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
