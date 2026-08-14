# SESSION_LOG.md

## 2026-07-19 16:12 UTC-04:00 – Initial bootstrap

- **Goal:** Bootstrap the repository for Milestone 0 (Compatibility and Toolchain).
- **Files changed:**
  - Created repository directory structure.
  - Created `.project-state.json`.
  - Created `README.md`.
  - Created `docs/handoff/*.md` scaffold.
- **Commands executed:**
  - `mkdir -p` directory structure.
- **Tests executed:** None.
- **Results:** Repository structure and handoff files created. No QNX toolchain verified yet.
- **Commits created:** None.
- **Problems discovered:** None.
- **Remaining work:** Create compatibility matrix, QNX CMake toolchain, host doctor, minimal QNX Screen application, ADRs, build-and-deploy docs, then validate.
- **Recommended continuation point:** `docs/handoff/NEXT_ACTIONS.md` Next Action 1.

## 2026-07-19 20:14 UTC – Milestone 0 scaffolding and initial validation

- **Goal:** Complete Milestone 0 scaffolding and validate toolchain detection on the available host.
- **Files changed:**
  - `docs/getting-started/compatibility-matrix.md` (created)
  - `docs/adr/index.md`, `adr-template.md`, `0001-0008` (created)
  - `cmake/toolchains/qnx-8.0.cmake` (created)
  - `CMakeLists.txt` (created)
  - `native/application/main.cpp` (created)
  - `native/screen/QnxScreenSurface.h` (created)
  - `native/screen/QnxScreenSurface.cpp` (created)
  - `scripts/doctor.sh`, `bootstrap-host.sh`, `verify-qnx-license.sh`, `configure-qnx.sh`, `build-qnx.sh`, `deploy-qnx.sh`, `run-qnx.sh`, `log-qnx.sh` (created and made executable)
  - `docs/getting-started/build-and-deploy.md` (created)
  - `package.json`, `pnpm-workspace.yaml`, `.gitignore` (created)
  - `artifacts/logs/.gitkeep` (created)
  - `docs/handoff/CURRENT_STATE.md`, `NEXT_ACTIONS.md`, `TEST_STATUS.md`, `ENVIRONMENT.md` (updated)
- **Commands executed:**
  - `bash scripts/doctor.sh` – completed; 9 expected errors, 5 warnings on macOS host without QNX SDP.
  - `bash scripts/configure-qnx.sh aarch64 Debug` – expected failure; CMake toolchain reported `QNX_HOST` not set.
- **Tests executed:**
  - `doctor.sh` diagnostic run.
  - `configure-qnx.sh` configure attempt.
- **Results:**
  - Doctor correctly identifies the unsupported macOS host and missing QNX toolchain.
  - Toolchain correctly produces a fatal configure error with exact remediation steps.
  - No actual QNX compilation performed because QNX SDP is not installed.
- **Commits created:** `f766782`, `731c822`, `0bfc174`, `af15694`.
- **Problems discovered:**
  - Current workspace is macOS arm64, not the supported x86-64 Ubuntu 24.04 build host.
  - QNX SDP 8.0.5 and Ninja are missing.
- **Remaining work:**
  - Install QNX SDP 8.0.5 on an x86-64 Ubuntu 24.04 host and re-run doctor/configure/build.
- **Recommended continuation point:** `docs/handoff/NEXT_ACTIONS.md` Next Action 1.

## 2026-08-13 23:00 EDT – Mac-first foundation completion and source audit

- **Goal:** Finish the interrupted macOS-host foundation, select the most current
  mutually compatible package line, correct concrete QNX Screen bugs, validate
  the host path, and leave an exact external-QNX handoff.
- **Dependency result:** Pinned pnpm 11.21.0, React Native 0.86.2, React 19.2.3,
  Expo 57.0.12, Metro 0.84.4, Nitro Modules/Nitrogen 0.36.5, TypeScript 6.0.3,
  Prettier 3.9.6, and current matching type packages. Regenerated
  `pnpm-lock.yaml`.
- **Registry audit:** Compared the selected line with registry `latest` tags on
  2026-08-13. RN 0.87.0, Metro 0.87.0, React 19.2.8, and TypeScript 7.0.2 were
  identified but not mixed into Expo 57's installed RN 0.86.2/React 19.2.3/Metro
  0.84.4/TypeScript 6.0.3 baseline.
- **Installed-source audit:** Recorded exact Fabric, Yoga, Hermes, Nitro,
  Nitrogen, Expo Modules, and Expo Metro files/symbols. Confirmed that their
  installed platform paths do not provide QNX integration.
- **Security audit:** Replaced xcode's vulnerable uuid 7.0.3 with a scoped,
  source-checked uuid 11.1.1 override. Recorded two remaining high `image-size`
  Metro-tooling advisories as UPSTREAM-DEP-001 because the registry has no fixed
  release.
- **Native corrections:** Relocated QNX sources under `native/platform/qnx`,
  fixed typed window creation and include paths, partial-init cleanup, `errno`
  propagation, ARGB fill attributes, render-buffer storage/re-query, and
  fill-to-present ownership.
- **Host coverage:** Added a test-only QNX Screen subset/fake plus eight wrapper
  cases, strict warnings, CMake presets, CTest wiring, ASan, and UBSan.
- **Scaffold/tooling corrections:** Made required empty directories trackable;
  replaced placeholder package/codegen success with explicit applicability
  results; added dependency, manifest, architecture, JS, and full-host runners;
  updated bootstrap/configure/build/deploy scripts.
- **Documentation/state:** Reconciled README, compatibility matrix, ADR-0008,
  build guide, project memory, project state, blockers, tests, queue, environment,
  current state, and next actions. Removed undocumented completed task IDs.
- **Commands executed:** Exact pnpm 11.21.0 install; registry metadata queries;
  `pnpm validate:macos`; standalone source/architecture checks; CMake/CTest host
  builds; `git diff --check`; Bash/JSON syntax checks; expected unsupported-host
  QNX configure/doctor diagnostics; `pnpm outdated`; and `pnpm audit`.
- **Tests:** Full host validation passed. CTest 1/1 passed, containing eight C++
  cases under ASan/UBSan. JS, package-build, and Nitrogen generation stages were
  explicitly not applicable because their inputs do not exist yet.
- **QNX result:** Not compiled or run. The expected configure diagnostic reports
  missing `QNX_HOST`; the installed SDK/header/source gate remains external.
- **Commits created:** None. The completed batch remains uncommitted.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 1 / QNX-RN-008 on
  the licensed Ubuntu/QNX host and target.

## 2026-08-14 – Batch verification and commit

- **Goal:** Resume the interrupted session, re-verify the outstanding
  working-tree batch, and commit it so the repository stops carrying unrecorded
  work.
- **Commands executed:** `pnpm validate:macos` (twice), `pnpm format`,
  `pnpm install --frozen-lockfile`, staged-diff secret grep, `git commit`.
- **Results:** The first validation run failed at stage 1 because
  `docs/cited-seams.md` and `docs/getting-started/compatibility-matrix.md` were
  not Prettier-clean. `pnpm format` fixed both. The second run passed all eight
  stages, with CTest 2/2 (`rnqnx_g0_cxx20_probe`, `qnx_screen_surface_host_test`)
  under ASan/UBSan. The lockfile was already in sync.
- **Commits created:** the Milestone 0 foundation batch.
  (`feat(foundation): lock Qt Quick backend and land mac-first host validation`).
  Nothing was pushed.
- **Problems discovered:** Only the two Prettier violations above. The previous
  session's `TEST_STATUS.md` CTest count of 1/1 was stale once the G0-A probe
  target landed; it is corrected in the 2026-08-14 section.
- **QNX result:** Unchanged. No QNX translation unit compiled, no QNX or Pi 4
  binary run. G0-A/G0-B/G0-C/G0-D remain blocked on the licensed SDP host and
  hardware in `BLOCKERS.md`.
- **Remaining work:** Unchanged and entirely external-prerequisite bound.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 1 / QNX-RN-008 on
  the licensed Ubuntu/QNX host and target.

## 2026-08-14 – Qt Quick host bring-up

- **Goal:** Stop treating all native work as blocked. Establish macOS as a
  development host for portable native code and implement the first real view
  seam against installed Qt.
- **Decisions:** ADR-0010 accepts macOS as a development host with four
  constraints (portable code only, platform code behind existing seams, host Qt
  symbols must exist in the Qt LTS that G0-B pins, optional `find_package`).
  ADR-0002 moves from proposed to provisionally accepted on host evidence.
- **Implemented:** `QtQuickItemFactory` (cached `QQmlComponent` per component
  type, inline QML type source, typed failure paths) plus
  `rnqnx_qtquick_host_probe`, wired into CTest under the offscreen QPA plugin.
- **Commands executed:** `brew list --versions qt`, header/symbol audit of the
  installed Qt frameworks, `node scripts/run-cpp-host-tests.mjs`,
  `pnpm validate:macos`.
- **Results:** Qt 6.11.1 resolves without a `CMAKE_PREFIX_PATH` hint. Both new
  targets build clean under `-Werror`. CTest 3/3. Probe:
  `items=1000 create_ms=12.856 update10_ms=0.456 destroy_ms=0.516`.
- **Problems discovered:** None in the new code. Fixed a duplicated word in a
  `CMakeLists.txt` comment introduced by an earlier documentation sweep.
- **QNX result:** Unchanged. No QNX translation unit compiled, no QNX or Pi 4
  binary run. G0-A, G0-B, G0-C, G0-D, and QNX-RN-008 remain blocked on the
  licensed SDP host and hardware in `BLOCKERS.md`.
- **Remaining work:** Fabric mounting layer and component-view registry against
  the installed React Native sources, then the JS/TS packages. Neither needs the
  QNX toolchain.
- **Recommended continuation point:** Fabric host/mount boundary, action 3 in
  `NEXT_ACTIONS.md`, now buildable on this host.

## 2026-08-14 – Fabric mounting layer, platform half

- **Goal:** Build the mounting layer's platform side on the development host,
  without waiting for the QNX toolchain.
- **Implemented:** `QtQuickMountingLayer` — create, insert, remove, destroy,
  updateLayout over Qt Quick items, with an operation set mirrored field-by-field
  from `ShadowViewMutation`. Tag and LayoutFrame are mirrored from
  `ReactPrimitives.h:17` and `LayoutMetrics.h:26` with the generic cxx `Float`
  (float), and all six mirrors are recorded in the cited-seams ledger.
- **Ownership:** items are owned individually rather than through QObject
  parentage. `QtQuickItemFactory` now states `QQmlEngine::CppOwnership`
  explicitly instead of relying on the default.
- **Commands executed:** installed-header audit of `ShadowViewMutation.h`,
  `ShadowView.h`, `ReactPrimitives.h`, `LayoutMetrics.h`, `Rect.h`, `Float.h`,
  and `qquickitem.h`; `node scripts/run-cpp-host-tests.mjs`; a deliberate
  mutation of the restacking branch; `pnpm validate:macos`.
- **Results:** CTest 4/4. Fifteen mounting cases pass under ASan/UBSan and strict
  warnings. Disabling the restacking branch failed
  `insertParentsAndOrdersChildren` at the expected assertion, confirming the
  ordering coverage is real; the mutation was reverted and the suite re-run
  green.
- **Problems discovered:** React Native's mounting headers require folly and
  glog, which are not vendored in the installed package. This blocks the
  `ShadowViewMutation::List` adapter on dependency recipes, not on hardware.
  Recorded as `NEXT_ACTIONS.md` action 3.
- **QNX result:** Unchanged. No QNX translation unit compiled, no QNX or Pi 4
  binary run.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — folly, glog,
  fmt, double-conversion, and boost recipes under `vnext/cmake/`, then the
  adapter.

## 2026-08-14 – React Native third-party recipes and the mutation adapter

- **Goal:** Remove the last non-hardware blocker by making React Native's
  renderer headers compile here, then write the adapter that was waiting on it.
- **Dependency recipes:** `cmake/dependencies/ReactNativeThirdParty.cmake`
  fetches folly 2024.11.18.00, fmt 12.1.0, glog 0.3.5, double-conversion 1.1.6,
  and fast_float 8.0.0 — every version read from the installed package, with
  SHA256 pins. Compile definitions, the glog `.h.in` token map, and glog's
  `config.h` are ported from React Native's own Android recipes.
- **Deliberate deviations:** boost is not fetched (nothing in the include graph
  reaches a boost-using folly header); folly is header-only (no `folly_runtime`
  source is needed to link what exists); `FOLLY_HAVE_XSI_STRERROR_R` and
  `FOLLY_HAVE_RECVMMSG` are omitted. All recorded in the cited-seams ledger.
- **Implemented:** `ShadowViewMutationAdapter` — a switch over
  `ShadowViewMutation::Type` applying Create, Delete, Insert, Remove, and Update
  to `QtQuickMountingLayer`, with per-mutation failures returned rather than
  thrown or swallowed.
- **Commands executed:** version audit of the installed podspecs and
  `helpers.rb`; tarball download and SHA256 capture;
  `node scripts/run-cpp-host-tests.mjs` across several iterations; a deliberate
  mutation of the adapter's Insert index; `pnpm validate:macos`.
- **Results:** CTest 6/6. React Native renderer headers compile for the first
  time. `rnqnx_rn_headers_probe` enforces every cited mirror with
  `static_assert`. Seven adapter cases pass. Forcing Insert to index 0 failed
  `insertOrderFollowsMutationIndex` as expected; reverted and re-run green.
- **Problems discovered:** `FetchContent_MakeAvailable` tried to configure the
  upstream CMake projects, several of which declare pre-3.5 minimums that
  current CMake rejects — solved with a non-existent `SOURCE_SUBDIR`. glog
  defines a `CHECK` macro that collided with the test harness. RN's
  debug-string-convertible code pulled `ShadowView.cpp` into the link, resolved
  with React Native's documented `REACT_NATIVE_PRODUCTION` switch.
- **QNX result:** Unchanged. No QNX translation unit compiled, no QNX or Pi 4
  binary run. These dependencies have not been cross-compiled for QNX and the
  QNX-side recipe work is untouched.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — a JSI runtime
  host plus a `SchedulerDelegate` so transactions come from a live surface.

## 2026-08-14 – React Native renderer builds from source on the host

- **Goal:** Build React Native's real renderer modules rather than compiling
  individual sources, so the mounting work sits on the actual Fabric machinery.
- **Result:** 41 modules build and link on macOS, added through React Native's
  own per-module `CMakeLists.txt` files so that source lists, flags, and link
  lists stay owned by React Native. `folly_runtime` now compiles all 29 sources
  React Native lists, alongside real glog, fmt, and double-conversion libraries
  and React Native's trimmed boost headers.
- **Problems discovered, each solved with React Native's own mechanism:**
  the platform selector had to be included before adding modules so the generic
  `cxx` paths are chosen; `REACT_ANDROID_DIR` had to be set so `target_merge_so`
  resolves, which is a no-op off Android; nine `platform/cxx` directories had to
  go on the global include path, matching React Native's assembler, because
  `react_renderer_componentregistry` reaches `HostPlatformViewProps.h` without
  linking `rrc_view`; Yoga's target is `yogacore` and needed a `yoga` alias; and
  OBJECT library objects are not archived into a static library that links them,
  so the aggregate target attaches `$<TARGET_OBJECTS:...>` explicitly.
- **Reverted:** `REACT_NATIVE_PRODUCTION`. `react_renderer_debug` defines
  `RN_ENABLE_DEBUG_STRING_CONVERTIBLE` publicly in Debug builds, so the flag
  would have split across translation units. Building the mounting module
  supplies the symbols the adapter needed.
- **Commands executed:** repeated `node scripts/run-cpp-host-tests.mjs` cycles
  driven by the exact missing target or header at each step; `pnpm
validate:macos`.
- **Results:** CTest 6/6, unchanged coverage, now running against React Native's
  real modules.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
  These modules have never been built with `qcc`.
- **Recommended continuation point:** drive a real `ShadowTree` commit through
  the Differ and `MountingCoordinator` into the adapter, using React Native's
  `react/renderer/element` builder, and assert Yoga's computed layout on the Qt
  items.

## 2026-08-14 – Shadow tree to Qt items, end to end

- **Goal:** Stop hand-building mutation lists and drive the Qt Quick items from
  React Native's real shadow tree, differ, and mounting coordinator.
- **Implemented:** `shadow_tree_mounting_host_test` — commits a real
  `ShadowTree` from C++, pulls the transaction through
  `MountingCoordinator::pullTransaction`, applies it with the existing adapter,
  and asserts Yoga's computed geometry on the Qt items. Five cases: fixed size,
  flex distribution with stacking offsets, padding offsets, in-place resize on a
  second commit, and unmount on removal.
- **Findings:**
  - View flattening. The first run produced exactly one mutation — an Update of
    the root, sized to the child — and no Create, because React Native flattens
    views carrying only layout props. The test sets `collapsable = false`
    (`BaseViewProps.h:107`) and the behaviour is now documented; it is React
    Native's design, not a defect, and the platform gets it for free.
  - `react/renderer/element/testUtils.h` cannot be used. Its
    `simpleComponentBuilder` registers modal and text descriptors that include
    generated `FBReactNativeSpec` headers, which this repository does not
    produce. The test registers Root and View descriptors directly.
  - `ShadowNode::ListOfShared` does not exist in 0.86.2; the type is spelled
    inline at `ShadowNodeFragment.h:27`.
- **Commands executed:** installed-header audit of `ShadowTree`,
  `ShadowTreeDelegate`, `MountingCoordinator`, `ShadowNodeFragment`, and the
  Yoga style setters; a temporary mutation dump to diagnose the missing
  mutations; `node scripts/run-cpp-host-tests.mjs`; a deliberate removal of
  `setY` from the mounting layer; `pnpm validate:macos`.
- **Results:** CTest 7/7. Removing `setY` failed `flexChildrenSplitTheRootWidth`
  (`0 != 40`) and `paddingOffsetsChildOrigin` (`0 != 16`) as expected; reverted
  and re-run green.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Remaining work:** JavaScript. No Hermes runtime, no bundle, no
  `UIManagerBinding`; the tree is committed from C++. Props beyond layout are
  unconsumed.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3, step 1 —
  Hermes at tag `hermes-v250829098.0.16` for the development host.

## 2026-08-14 – Hermes on the host, and the runtime/renderer audit

- **Goal:** Build the JavaScript engine React Native 0.86.2 actually uses, and
  establish exactly what stands between a running runtime and mount
  transactions.
- **Version resolved:** the package carries two Hermes versions and the newer is
  the default — every build path opts _out_ of v1 rather than into it
  (`hermes-utils.rb:89-91`, `PrivateReactExtension.kt:62`,
  `version.properties:1-2`, `package.json:179`). So
  `hermes-v250829098.0.16`, not `0.17.0`.
- **Pinning:** the tag is pinned to commit `90f23852`, verified at configure
  time. A tarball SHA256 was rejected as a pin because GitHub's generated
  archives are not byte-reproducible.
- **Built:** 418 targets, `libhermesvm.dylib`, no errors.
  `rnqnx_hermes_probe` asserts arithmetic, an ES6 shape, an async arrow body
  executing, and a host function crossing JSI in both directions.
- **Problem found and fixed:** React Native ships two CMakeLists for JSI that
  are not equivalent — the outer globs `jsi/*.cpp` (including `JSIDynamic.cpp`),
  the inner builds `jsi.cpp` alone. Hermes adds the inner one, so
  `AppRegistryBinding` failed to link on `valueFromDynamic`. Added that one
  translation unit back rather than duplicating a JSI copy.
- **Audit result:** everything from `Scheduler` downward is generic C++. Five
  pieces have no generic implementation and are ours to write: an `EventBeat`
  subclass, a `ComponentRegistryFactory` (its supposed default,
  `getDefaultComponentRegistryFactory()`, is declared and never defined), a
  `SchedulerDelegate` subclass, a `bindHasComponentProvider` predicate, and a
  `MessageQueueThread`/`PlatformTimerRegistry` pair. Recorded in
  `docs/cited-seams.md` and `NEXT_ACTIONS.md` action 3.
- **Commands executed:** version audit across the installed package; tag and
  commit verification via `git ls-remote`; Hermes configure and build; a
  deliberate mutation of the async-arrow probe; `ctest` in both configurations;
  `pnpm validate:macos`.
- **Results:** CTest 7/7 default, 8/8 with `-DRNQNX_BUILD_HERMES=ON`. Emptying
  the async arrow's body failed the probe as expected; reverted and re-run
  green.
- **QNX result:** Unchanged. Hermes has not been cross-compiled with `qcc` and
  has not run on the Pi 4. The QNX build will additionally need the two-stage
  `IMPORT_HOST_COMPILERS` bootstrap that host builds skip.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — the five
  pieces above, in the order given.

## 2026-08-14 – Runtime and renderer wired together

- **Goal:** Implement the pieces React Native ships no generic implementation
  of, and get a surface commit to travel from React Native's UIManager to Qt
  Quick items.
- **Implemented:** `QtEventBeat` (ticks off the Qt event loop),
  `makeComponentRegistryFactory()`, `QtSchedulerDelegate` (all thirteen pure
  virtuals; mounts from `schedulerShouldRenderTransactions`), and `QtReactHost`,
  which owns the Hermes runtime, the `RuntimeScheduler`, the `ContextContainer`
  and React Native's `Scheduler`.
- **Avoided rather than faked:** `MessageQueueThread` and
  `PlatformTimerRegistry` are `ReactInstance` requirements, and the host drives
  the runtime directly, so neither is needed yet. `bindHasComponentProvider` is
  only needed once JavaScript resolves view configs. Both are recorded as
  remaining work.
- **Problems discovered, each traced to installed source:**
  - Registering `UnimplementedViewComponentDescriptor` as the fallback
    segfaulted. Its `getComponentName()` dereferences `flavor_` as a
    `std::string` (`UnimplementedViewComponentDescriptor.cpp:17-19`) and
    `setFallbackComponentDescriptor` registers immediately. React Native's own
    default is `UnimplementedNativeView`, which is what Android installs.
  - `createComponentDescriptorRegistry` returns a pointer-to-const; the fallback
    goes in through a `const_cast`, matching
    `DefaultComponentsRegistry.cpp:54-56`.
  - Stopping an empty surface threw, because `UIManager::stopSurface` calls
    `AppRegistryBinding::stopSurface` unconditionally. The host installs a
    `RN$stopSurface` no-op only when the global is absent.
  - Three renderer modules require `react_codegen_rncore`, which is codegen
    output with no CMakeLists. Built from the pre-generated copy React Native
    ships under `React/FBReactNativeSpec`, whose renderer half is plain C++.
- **Commands executed:** iterative configure/build cycles driven by the exact
  missing target; `lldb` and checkpoint bisection to locate the segfault; a
  deliberate mutation of the delegate's mount call; `ctest` in both
  configurations; `pnpm validate:macos`.
- **Results:** CTest 9/9 with Hermes, 7/7 default. Making the delegate apply an
  empty mutation list failed three cases as expected; reverted and re-run green.
  A first attempt at that mutation did not compile, so its apparent pass was a
  stale binary rather than evidence — re-run with a compiling mutation.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — a bundle,
  which needs `bindHasComponentProvider` and a minimal JS entry point first.

## 2026-08-14 – JavaScript drives the renderer

- **Goal:** Close the last gap that does not need a bundler: let JavaScript
  build a view tree and have it mount.
- **Implemented:** `installComponentProvider()` binds React Native's component
  predicate (`bindHasComponentProvider`), normalising React view names through
  `componentNameByReactViewName` so `RCTView` resolves to `View`. The answer
  comes from `registeredComponentNames()`, one list shared with the registry
  factory, because `ComponentDescriptorRegistry` has no name-based existence
  check and a fallback makes `at()` unable to answer.
- **Proven:** a script calling `nativeFabricUIManager.createNode`,
  `createChildSet`, `appendChildToSet`, and `completeRoot` — the same binding
  surface React's renderer drives — mounts a Qt Quick item sized by Yoga from
  props JavaScript supplied.
- **Commands executed:** audit of `UIManagerBinding.cpp` for the exposed JS
  methods and their argument shapes; added the `rrc_native` module for the
  predicate binding; build and `ctest` in both configurations; a deliberate
  mutation of the expected width; `pnpm validate:macos`.
- **Results:** CTest 9/9 with Hermes, 7/7 default. Expecting 161 where
  JavaScript passed 160 failed the case, so the value really travels from JS
  through Yoga to the item.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Remaining before a real app:** Metro, an entry point installing
  `global.RN$AppRegistry`, timers/JS thread if `ReactInstance` is adopted, and
  props beyond layout.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3, step 1 —
  `metro-config-qnx`.
