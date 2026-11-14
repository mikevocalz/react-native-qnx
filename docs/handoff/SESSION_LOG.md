# SESSION_LOG.md

## PR 159 review follow-up — 2026-11-14

Reviewed the actual d862625 source and open findings, reproduced both CI failures, and implemented narrow image/router/consumer fixes with failing old-source controls. Independently reviewed and integrated the keyboard/modal corrections. Host Qt and affected JS tests pass; a real generated external app bundles for both targets. Full-JS socket restrictions, pending full-host checks and absent current-target evidence are recorded without promoting a release gate.
[Source, commands, limits and rollback](../validation/pr159-review-followup-2026-11-14.md).
Main/SVG integration includes PR 161 follow-up `13a0e49`; both evidence sets are retained. Transactional imageformat staging reads the selected `IMAGEFORMATS_SRC` notices and all attribution inputs; 25 host staging fixtures pass with a failing old-source control. The final code source is `252b128`; its complete source-tree identities and 55 SHA-256 entries are recorded in the receipt.

Independent review corrected the initially blank paused animation, with failing direct and deleted-source prefetch controls. Fresh image Debug ASan/UBSan passes 3/3 CTests and 73 QtTest entries. Reconciliation found a stale generated Release MOC that omitted the new disk-clear HTTP slot; after refreshing only those build artifacts, all 6/6 Release CTests and 126 entries pass. Prior 125-entry logs remain qualified and preserved. Pinned Hermes and Metro builds succeeded, but GCC stopped on a Clang-specific upstream React Native pragma before linking the host test; the supported Clang retry and both new host regressions remain pending. Exact-head CI and current-target gates remain open.

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

Read the November 13 handoff and current failed CI. Fixed the missing video command-handler initializer and validated the bounded table/JS scope. No Pi app or boot change was made.
[Commands and limits](../validation/pr159-table-initializer-2026-11-13.md).

## PR 161 nested SVG and staging integration (2026-11-14)

Codex inspected the current PR, prior review threads and failing corpus logs;
kept timeouts and tolerances unchanged; and corrected two missed nested-root
cases. New scene tests fail for the intended reasons against the original
source and pass on published native source `3324e7c`. The coordinating agent
reviewed those changes. Main merge `446d28f` preserves PR 163's transactional
staging while bounding optional SVG sources; a separate reviewer found no
blocker and independently passed all 17 fixtures.

The exact Qt 6.11.1 Linux Debug ASan/UBSan run exposed a GCC test construction
bug, fixed by reading kernel size before moving the vector. All original pixel
assertions remain, and the final standalone suite passes 8/8. Typecheck,
documentation validation and 47 target-doctor host scenarios pass. Recorded
raw failures/successes, build flags, source hashes and remaining qualification
in the [receipt](../validation/pr161-nested-roots-2026-11-14.md), updated this
handoff set and the canonical readiness evidence, and regenerated project
state. No current QNX or device result is claimed; keep the native PR draft
pending the documented target work. External CodeRabbit authentication was
unavailable; the receipt identifies agent reviews separately.

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

Read current PR 161 CI, reproduced the corpus in a separate Linux build, verified standalone Mac rendering, isolated large scenes and added required shard coverage. Kept the original reference images and recorded the Use-2 edge tolerance measurement.
[Receipt](../validation/pr161-corpus-shards-2026-11-13.md).

## 2026-11-10 — PR 105 committed handoff repair

Inspected `c09a5b1`, its green hosted workflows, the unresolved handoff review,
the repository validation/API skills and ordered pinned Margelo sources.
Re-ran generator/type/source checks, recorded the exact lock and CI runs, and
updated all six required handoff artifacts through the canonical readiness
record and `pnpm docs:sync`. No implementation or acceptance gate changed.
[Full receipt](../validation/pr105-merge-readiness-2026-11-10.md).

## 2026-10-04 — Replace arithmetic-only React QNX probe

- Replaced UIManager-plus-arithmetic success with a real bundled React surface,
  initial geometry/colors, state update, and unmount assertions.
- Shared C++/Metro implementation is built for the host and QNX; host CTest
  includes missing/empty/throwing/unregistered-bundle rejection.
- Review hardened unmount against detached-but-leaked items and a no-op React
  cleanup; the positive CTest wrapper checks exit status as well as stdout. Output labels its platform
  and geometry-only scope; source validator does not claim execution.
- Source/TypeScript/JS/codegen/architecture/package checks and production Metro
  bundling pass. Basic host smoke passes 2/2, with Qt graph skipped there.
- Found and fixed the probe's Qt 6.11 FINAL-property redeclarations. Real
  initial mount then exposed missing scheduler task boundaries; companion PR #28
  `24ba958d046066f54faae30847de1c52b972f4ba` fixes them.
- Actual shared probe CMake target and positive/four-negative runners passed
  2/2; the combined native suite passed 11/11 CTests including 45 host cases.
  Restored the companion checkout after testing. Main alone is not a verified
  passing runtime baseline without #28. Target toolchain/device/pixels remain
  unverified. Draft PR, no merge.

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
  - QNX SDP 8.0.2 and Ninja are missing.
- **Remaining work:**
  - Install QNX SDP 8.0.2 on an x86-64 Ubuntu 24.04 host and re-run doctor/configure/build.
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

## 2026-08-14 – Metro bundles for qnx

- **Goal:** Produce a real bundle for the platform and find out exactly what
  stands between it and rendering.
- **Implemented:** `@rn-qnx/metro-config-qnx`, which appends `qnx` to Metro's
  `resolver.platforms` — without it Metro never prefers a `.qnx.js` file, which
  is the whole out-of-tree mechanism. `apps/rn-playground` is a minimal entry
  that imports `AppRegistry` and `View` directly rather than all of
  `react-native`. CMake bundles it during the build as an artifact.
- **Host globals:** `RN$Bridgeless`, an empty `__fbBatchedBridgeConfig`, and
  `RN$registerCallableModule`, each installed before evaluation and each traced
  to the JavaScript that branches on it.
- **Problems discovered:**
  - Metro could not resolve `react` under pnpm. Package files live in
    `node_modules/.pnpm`, so the app config needs `watchFolders` and
    `nodeModulesPaths` pointing at the workspace root.
  - Evaluating the bundle threw `__fbBatchedBridgeConfig is not set`. The
    bridgeless flag alone does not gate that check
    (`NativeModules.js:185-192`); an empty remote module config is the truthful
    answer for a platform with no legacy bridge.
  - Then `undefined is not a function` from
    `registerCallableModule.js:20`, which needs the host-installed registrar.
  - Then `TurboModuleRegistry.getEnforcing('ExceptionsManager')`. That is where
    this stops.
- **Deliberately not claimed:** the bundle does not evaluate. The test asserts
  what Metro produced and the host installed, and the nine required TurboModules
  are enumerated in `NEXT_ACTIONS.md`. Asserting the failure would have been
  asserting a failure.
- **Results:** CTest 9/9 with Hermes, 7/7 default; `validate:manifests` reports
  two real manifests.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — check which
  of the nine modules React Native already ships as generic C++ before writing
  anything.

## 2026-08-14 – Version correction: QNX SDP 8.0.2, Qt 6.11.1

- **Trigger:** the project owner reported that QNX is on SDP 8.0.2 and Qt is on
  6.11.1 with 6.12 in beta.
- **Defect found:** six handoff documents claimed "QNX SDP 8.0.5". That version
  was never verified against anything — this workspace has never had a licensed
  SDP — and appears to have been written from assumption in an early session.
  It is exactly the kind of invented specific this repository is supposed to
  refuse.
- **Corrected:** every `8.0.5` reference now reads `8.0.2`, marked as
  owner-supplied and still subject to the G0-A capture of the installed package
  identity, which overrides it. `docs/toolchain.md` gains a Versions section
  recording both the correction and its cause.
- **Qt:** 6.11.1 is the current release and is what the macOS development host
  already builds against, so no code changes. ADR-0010 and the compatibility
  matrix now say "current release" rather than implying 6.11.1 is unusual, and
  note 6.12 in beta. The LTS the target pins is still a G0-B decision, and the
  gap between it and 6.11.1 is what ADR-0010's symbol re-audit covers.
- **Not changed:** no build, test, or code path depends on either number. The
  Qt the host compiles against is read from the installation, not from a
  document.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.

## 2026-08-14 – The bundle evaluates in Hermes

- **Goal:** Satisfy the TurboModule lookups React Native's own JavaScript makes,
  and find the next real blocker.
- **Architecture check:** this is ADR-0004's sanctioned adapter, not a move to
  TurboModules. That ADR already says application-facing APIs are Nitro and that
  "a minimal internal adapter satisfies React Native core interfaces that expect
  TurboModule registry semantics," delegating real behaviour to Nitro or the
  platform runtime, with the note not to generate empty TurboModules. The
  provider delegates to `DefaultTurboModules::getTurboModule` and returns
  nullptr for anything else, so an unimplemented module fails by name rather
  than receiving a stub.
- **Implemented:** `installTurboModuleBinding()`, sixteen more React Native
  modules on the build (nativemodule core/defaults/microtasks/idlecallbacks and
  their dependencies, bridging, animated, the observers), and the include roots
  those targets assume are global.
- **Result:** the Metro bundle now evaluates in Hermes and installs
  `global.RN$AppRegistry`.
- **Problems discovered:**
  - Qt defines `emit` as a macro and React Native's
    `react/bridging/EventEmitter.h` declares a method called `emit`. Fixed with
    `QT_NO_KEYWORDS`, which costs nothing here.
  - `TurboModuleRegistry.js:17` captures `global.__turboModuleProxy` once at
    module-eval time, so the binding has to be installed before any bundle runs.
  - Rendering still fails: `Cannot read property 'select' of undefined`, because
    `Libraries/Utilities/Platform.js` re-exports `./Platform` and there is no
    `Platform.qnx.js`. That is the JavaScript platform package, not a native
    gap.
- **Not claimed:** the render test was written, run, and then rewritten to
  assert only what actually happens — the bundle evaluates. Asserting a render
  would have been asserting a failure.
- **Results:** CTest 9/9 with Hermes, 7/7 default; twelve host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 —
  `Platform.qnx.js` in `@rn-qnx/react-native-qnx` plus a Metro
  `resolveRequest` redirect.

## 2026-08-14 – A React bundle renders to Qt Quick items

- **Goal:** Close the last gap between a bundle and a rendered tree.
- **Result:** `metroBundleRendersThroughReact` passes. Metro bundles the
  playground for `qnx`, Hermes evaluates it, `startSurface` runs
  `RN$AppRegistry.runApplication`, React reconciles, and the tree lands as Qt
  Quick items with the geometry the JSX declares — a 320x180 outer item
  containing a 64x32 inner item, computed by Yoga.
- **Implemented:** `@rn-qnx/react-native-qnx` with `Platform.js` and
  `BaseViewConfig.js`; a Metro `resolveRequest` in `metro-config-qnx` that runs
  Metro's resolver first and swaps the result when an override exists at the
  same relative path, which is what makes relative imports such as `./Platform`
  work; and four host responsibilities — Hermes' microtask queue, `QTimer`-backed
  timers, explicit draining, and platform constants from `QSysInfo`.
- **Problems discovered, in the order the runtime found them:** `Platform` was
  undefined; `setTimeout` did not exist; microtasks were disabled in the Hermes
  runtime; the view config was undefined. Each was a real missing host or
  platform responsibility, and each was fixed rather than stubbed.
- **Mutation checks:** changing the app's width to 300 fails the render test
  with `300 != 320`, so the value travels from JSX through Metro, Hermes, React,
  Yoga and the mounting layer. Flipping `Platform.OS` to `ios` changed nothing —
  recorded, because it means `OS` is not load-bearing for this path and a future
  test should not assume otherwise.
- **Also fixed:** the bundle build now depends on the platform package and Metro
  config sources, so editing them no longer leaves a stale bundle. That
  staleness cost a debugging cycle.
- **Results:** CTest 9/9 with Hermes, 7/7 default, thirteen host cases;
  `validate:manifests` reports three real manifests.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — props beyond
  layout, then ExceptionsManager, then render on glass.

## 2026-08-14 – Visual props, and a vacuous test caught by mutation

- **Goal:** Make the rendered tree visible, not just correctly sized.
- **Implemented:** `ViewStyle` and `QtQuickMountingLayer::updateStyle`, applying
  background colour, corner radius, border, and opacity. The adapter fills it
  from `BaseViewProps` using React Native's own
  `resolveBorderMetrics(layoutMetrics)`, so the border cascade — including
  percentage radii against the frame — is React Native's implementation rather
  than a second one written here.
- **Limits recorded, not hidden:** Qt Quick's `Rectangle` has a single `radius`
  and a single border pen, so per-corner radii collapse to the top-left value
  and only the left border edge is read. Both are in
  `docs/component-support.md`, which is new.
- **A mutation check caught a vacuous test before it was committed.** The script
  that was supposed to add the colour assertions patched the app first, asserted
  on text prettier had already reformatted, and threw — so neither file was
  written, yet the suite passed. Mutating the app's colour also passed, which is
  what exposed it. The assertions are now present and the same mutation fails.
  Worth remembering: a passing suite after an edit is not evidence the edit
  landed.
- **Results:** CTest 9/9 with Hermes, 7/7 default.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 —
  ExceptionsManager next, then render on glass.

## 2026-08-14 – ExceptionsManager

- **Goal:** Stop JavaScript errors from killing the process while reporting
  themselves.
- **Implemented:** `QnxExceptionsManager` over the generated
  `NativeExceptionsManagerCxxSpec`, wired into the host's TurboModule provider
  ahead of the `DefaultTurboModules` fallback. It flattens React Native's stack
  frame objects into text, records each error on the host, and logs it.
  `dismissRedbox` is an honest no-op — there is no redbox here — rather than an
  unimplemented method.
- **Tests:** an error reported from JavaScript reaches the host with its
  message, fatal flag, and a stack that still names the method and file; and a
  healthy render of the playground bundle reports nothing.
- **Mutation check:** dropping the reporter call fails the first test on
  `errors.size() == 1`.
- **Results:** CTest 9/9 with Hermes, 7/7 default, fifteen host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — render on
  glass in a real window, then input.

## 2026-08-14 – On glass

- **Goal:** Prove the rendered tree is painted, not merely configured, and give
  a person something to look at.
- **Implemented:** `renderedTreeIsPainted` shows a `QQuickWindow`, renders the
  playground bundle, grabs the frame, and reads pixels. Also
  `rnqnx_playground_host`, which runs the same app in a real window or writes a
  PNG with `--screenshot` for machines with no display.
- **Asserted from pixels:** the outer view's centre is exactly `#204080`; a
  point outside it is the window's white backdrop, so the view is bounded; and
  the inner view's pixel sits between the two colours, which is the 50% opacity
  being composited by the renderer rather than only stored on the item.
- **Useful discovery:** `grabWindow` works under the offscreen platform plugin,
  so the pixel test runs headless and does not need a display.
- **Mutation check:** changing the app's colour to `#804020` fails on
  `outer.red() == 0x20`.
- **Looked at it:** the rendered frame is a 320x180 blue rounded rectangle with
  a half-transparent orange child in its corner, on white — which is what the
  JSX says.
- **Also updated:** the README's scope section, which still described the
  repository as a Screen probe with no renderer.
- **Results:** CTest 9/9 with Hermes, 7/7 default, sixteen host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — input, so
  the rendered app can respond to something.

## 2026-08-14 – Touch reaches JavaScript

- **Goal:** Make the rendered app respond to a pointer.
- **Implemented:** `QtTouchDispatcher` — a Qt event filter on the window that
  hit-tests to the deepest mounted item and calls that shadow view's
  `TouchEventEmitter`. React Native ships no generic input path; Android reads
  Choreographer motion events and Apple reads UIKit touches. The adapter
  registers each view's emitter as mutations arrive and forgets it on delete.
- **Test:** a `QMouseEvent` press and release on the window run the playground's
  `onTouchStart` and `onTouchEnd` handlers in JavaScript, and
  `event.nativeEvent.locationX` is the pressed coordinate.
- **Two bugs found, both mine:**
  - The dispatcher was a local in the test while the adapter held a raw pointer
    to it. Stopping a surface commits an empty tree _during host teardown_,
    which reaches the adapter and then the freed dispatcher — heap corruption
    surfacing as `EXC_BREAKPOINT` inside malloc, not a clean failure. The
    fixture now owns it, declared before the host, and the lifetime requirement
    is documented on `setTouchDispatcher`.
  - The harness printed `[PASS]` to buffered stdout, so the crash discarded the
    record of which cases had passed and made it look like the first case
    failed. It flushes now. That cost a diagnostic cycle chasing the wrong test.
- **Scope stated:** one pointer. No multi-touch, no pointer capture, no JS
  responder negotiation, so `Pressable` is marked partial rather than working.
- **Mutation check:** removing the `onTouchStart` call fails the case.
- **Results:** CTest 9/9 with Hermes, 7/7 default, seventeen host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — finish input
  (responder negotiation), then DeviceInfo and SourceCode.

## 2026-08-14 – The JavaScript responder system

- **Goal:** Get React's responder negotiation working, so gestures are possible.
- **Implemented:** responder event props in the platform's `BaseViewConfig` —
  React's Fabric renderer only attaches those handlers if the view config
  declares them — and `schedulerDidSetIsJSResponder` forwarding the claim to
  `QtTouchDispatcher`, which then routes the rest of a gesture to the responder
  rather than to whatever is under the pointer.
- **Result:** a press asks `onStartShouldSetResponder`, the view claims it,
  `onResponderGrant` runs, and the release delivers `onResponderRelease`. That
  is the machinery `Pressability` and every gesture are built on.
- **Mutation check:** returning `false` from `onStartShouldSetResponder` fails
  the case on `__granted`.
- **A misleading result, corrected:** the plan was to prove this by rendering a
  `Pressable` and asserting `onPress`. The test failed, and the obvious reading
  was that `Pressable` does not work. It was wrong: the scripted edit that added
  `Pressable` to the app never landed, so the run was testing an app with no
  `Pressable` in it. `docs/component-support.md` marks `Pressable` untested
  rather than broken, and `NEXT_ACTIONS.md` says to try it properly.
- **Process note:** that is the third silently-dropped edit this session to
  produce a misleading result — the earlier two were a vacuous colour assertion
  and a stale bundle. Every scripted edit now gets grepped back afterwards
  rather than trusted because the surrounding command succeeded.
- **Results:** CTest 9/9 with Hermes, 7/7 default, seventeen host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — render a real
  `Pressable`, then DeviceInfo and SourceCode.

## 2026-08-14 – Pressable

- **Goal:** Settle the previous entry's open question by actually rendering a
  `Pressable`.
- **Result:** it works. The playground renders one and `onPress` fires from a
  real `QMouseEvent` press and release, through React's responder system and
  Pressability's timers. It is the first component rendered on this platform
  that is not a bare `View`.
- **How it announced itself:** `metroBundleRendersThroughReact` failed on
  `outer->childItems().size() == 1`, because the mounted `Pressable` made it
  two. That failure was the proof the component had rendered.
- **Mutation check:** moving the press to a point outside the `Pressable` fails
  the case, so the assertion is about the hit and not merely about a press
  happening somewhere.
- **The earlier conclusion was wrong, and is corrected:** `Pressable` was never
  broken. The previous run tested an app the edit had failed to reach.
  `docs/component-support.md` moves it from ❓ to ✅. Verifying each scripted
  edit with a grep — added as a habit after the third such incident — is what
  made this run trustworthy.
- **Results:** CTest 9/9 with Hermes, 7/7 default, seventeen host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — `Text`, the
  most conspicuous remaining gap.

## 2026-08-14 – Text

- **Goal:** Display a string, the most conspicuous remaining gap.
- **Implemented:** the Paragraph/Text/RawText descriptors registered, the
  `TextLayoutManager` placed in the `ContextContainer` under the name
  `BaseParagraphComponentDescriptor.h:23` looks for, the paragraph's
  `AttributedString` flattened onto a Qt Quick `Text` item, and the first
  fragment's font size and colour carried across.
- **The measurement is ours.** React Native's generic C++ `TextLayoutManager`
  returns `layoutConstraints.minimumSize` — it does not measure — so every
  `<Text>` would have collapsed to nothing. This platform substitutes its own,
  measured with `QFontMetricsF`, the same substitution Android and Apple make.
  React Native's header is unchanged; only the definitions are replaced.
- **Mutation check:** changing the app's `fontSize` to 12 fails the test on the
  item's pixel size.
- **A bug found by looking at the screenshot:** the first render showed no text,
  even though every test passed. `rnqnx_playground_host` registered only the
  `View` QML type and never mapped `Paragraph`, while the test fixture had both.
  Two entry points, one of them missing setup — the tests could not have caught
  it. Fixed in the runner.
- **Gaps recorded, not hidden:** one style per paragraph, because Qt Quick's
  `Text` styles the whole item; single-line measurement, so wrapped text reports
  one line's height; no ellipsis for `numberOfLines`; zero-sized attachments.
- **Results:** CTest 9/9 with Hermes, 7/7 default, seventeen host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — finish
  `Text` (mixed styling, wrapping) or move to `Image`.

## 2026-08-14 – Text wrapping and mixed styling

- **Goal:** Finish `Text` — measure wrapped text correctly and keep per-fragment
  styling.
- **Measurement:** moved from `QFontMetricsF` to `QTextLayout` with a
  `FormatRange` per fragment, so a paragraph with mixed fonts measures correctly
  and long text wraps against the available width. React Native passes infinity
  for an unconstrained axis, which `QTextLine::setLineWidth` cannot take, so
  that case is guarded.
- **Rendering:** mixed styling is carried as Qt rich text, because Qt Quick's
  `Text` applies one font and colour to the whole item. Single-style paragraphs
  stay plain text so markup characters in ordinary strings are never
  interpreted.
- **A mismatch caught by looking:** the first render measured two lines but
  painted one, with the text running outside its own frame, because the item's
  own `wrapMode` was never set. Measurement and paint have to agree; the
  screenshot is what showed they did not.
- **Method correction, twice earned.** Two mutation runs reported a false pass
  because the mutated source failed to compile and the previous binary was still
  on disk. Mutation runs now check the build's exit status before the test's.
  With that in place: forcing the plain-text branch fails the rich-text
  assertion, and setting the measurer to `NoWrap` fails the wrapped-height
  assertion.
- **Results:** CTest 9/9 with Hermes, 7/7 default, seventeen host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — `Image`.

## 2026-08-14 – Image

- **Goal:** Render an `<Image>`.
- **Three prerequisites, each found by running it rather than by reading:**
  - `Image` is a fourth platform-resolved JavaScript file, after `Platform` and
    `BaseViewConfig`. Without an override the import is undefined and React
    throws "Element type is invalid". The override is deliberately minimal and
    its omissions are listed in `docs/component-support.md`.
  - `SourceCode` is needed before an image resolves, because `getDevServer`
    reads `scriptURL`. `QnxSourceCode` reports the bundle the host evaluated.
  - Qt Quick's `Image` does not read `data:` URIs; the item stays in `Loading`
    forever. `QnxImageProvider` decodes them and serves them back through an
    `image://` URL, which is Qt's supported mechanism and was already the plan
    recorded in `NEXT_ACTIONS`.
- **A stub left as a stub, deliberately:** React Native's generic C++
  `ImageManager` does not fetch or decode. That is correct here — Qt loads the
  source itself — so it is used as-is rather than replaced, and the reason is
  recorded where it is registered.
- **Also fixed along the way:** the platform package's `Image.js` avoided object
  rest destructuring, which Babel lowers to an `@babel/runtime` helper that is
  not installed; adding a runtime dependency for one line was not worth it.
- **Evidence:** the test asserts the provider URL, the styled size, and that the
  item reaches `Image.Ready`; the rendered frame contains exactly 960 magenta
  pixels, which is 40x24. Mutation check with the build verified first: making
  the source resolver a pass-through fails the case.
- **Results:** CTest 9/9 with Hermes, 7/7 default, seventeen host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 —
  `ScrollView` over Qt's `Flickable`.

## 2026-08-14 – ScrollView, and the module-architecture decision

- **Decision recorded:** asked directly whether these modules should be Nitro,
  the project owner chose that modules React Native's own JavaScript requires by
  name stay C++ against React Native's generated specs, while everything an
  application calls is a Nitro HybridObject. The reason it cannot be otherwise:
  `TurboModuleRegistry.getEnforcing` reads a global that Nitro's registry does
  not populate, and the call sites are inside React Native's source. Written up
  in `docs/handoff/DECISIONS.md` and noted on ADR-0004.
- **Implemented:** `ScrollView` mapped to a Qt `Flickable`, `DeviceInfo` as a
  C++ module reporting Qt's primary screen, and two platform behaviours:
  children of an item exposing a `contentItem` are mounted there, because that
  is what a Flickable actually scrolls; and `contentWidth`/`contentHeight` are
  recomputed on every insert, remove, and child layout change, because Qt does
  not derive them from children the way React Native does.
- **Problems discovered:**
  - `Dimensions.js:126` reads `getConstants().Dimensions`, so a flat constants
    object throws "Cannot read property 'screen' of undefined".
  - The scroll view first laid out at zero height. React Native's `ScrollView`
    base style is shrinkable and the parent's children already overflowed it, so
    Yoga collapsed it — correct behaviour, fixed in the app with
    `flexShrink: 0`, not in the platform.
- **Evidence:** the rendered frame contains exactly 5000 purple pixels, which is
  100x50 — the 300pt-tall content clipped to its viewport. Mutation check with
  the build verified first: removing the `contentItem` redirect fails the case.
- **Results:** CTest 9/9 with Hermes, 7/7 default, seventeen host cases.
- **QNX result:** Unchanged. Nothing cross-compiled, nothing run on the Pi 4.
- **Recommended continuation point:** `NEXT_ACTIONS.md` action 3 — try a
  `FlatList`, and deliver scroll events to JavaScript.

## FlatList virtualises, and scroll events reach JavaScript — 2026-08-14

Scroll offsets now travel from the Qt `Flickable` back to JavaScript as
`onScroll`, and `FlatList` was run to find out what it actually needed rather
than to confirm a guess.

`FlatList` needed nothing native beyond that. `onLayout` already worked and
`onContentSizeChange` already followed from the content container's layout
event, so once `onScroll` existed the list windowed correctly: with
`initialNumToRender: 4`, `windowSize: 1`, and a 100pt viewport scrolled to 300,
it renders the retained initial region and a live window around the offset and
releases the rows between.

Two things about method are worth carrying forward. First, the initial
`FlatList` assertions were wrong about the platform in both directions —
expecting fewer than 20 cells under a `windowSize` that covers three times the
content, and expecting the deliberately retained initial cells to be released.
Measuring the offsets and reading upstream's behaviour replaced both guesses.
Second, a fixed number of pump iterations is not a substitute for time:
`VirtualizedList` batches behind a real 50ms `setTimeout` and this platform's
timers are `QTimer`s, so the test pumps wall-clock time.

All five scroll and virtualisation mutants were confirmed to compile before
their failures were believed, per the correction recorded earlier in this log.

## TextInput, and a use-after-free the tests found — 2026-08-15

The native TextInput seam works: props apply, typing reaches JavaScript as
`onChange`. React Native's `TextInput.js` still renders nothing on `qnx`
because it branches on `Platform.OS` with no third branch, so the wrapper
remains open work.

The more important outcome was a latent use-after-free in the event beat,
which had been there since the beat was written and only became visible once
the new tests pumped the loop harder. Two lessons are worth carrying forward.
An intermittent failure is a defect until proven otherwise — this one was 3 in
40 and it would have been easy to re-run and move on. And a test that passes
when the bug is reintroduced is not a test: the dedicated regression case
written for this was deleted rather than kept for reassurance, and the
detection it claimed was replaced with a measured one under ASan.

## 2026-10-02 – Hardware-ready QNX cleanup

- **Goal:** Finish all non-hardware work before purchasing the Raspberry Pi and
  turn the first physical session into a deterministic validation run.
- **Repository state entering the session:** PRs #7–#12 were already merged;
  no pull requests were open.
- **Reconciled implementation:** current SDP 8.0.5 cross-build tooling, RN 0.88
  Hermes QNX source rebase, evidence verifier, synchronous Nitro QNX runtime,
  Expo Modules QNX JSI bootstrap, and Expo QNX autolinking are all in-tree.
- **Hardware-lane hardening:** `scripts/install-pi.sh` now defaults to three
  launch/exit cycles, requires an init/clean-shutdown pair for every run, writes
  `run-count.txt`, and invokes the evidence verifier automatically.
- **Evidence verifier:** supports historical one-run evidence while validating
  explicit repeated-run counts and records `runCount` /
  `checks.repeatedRuns` in the manifest.
- **Tests:** the synthetic Pi evidence test now covers a three-run pass and
  corrupted/incomplete run output.
- **Documentation repaired:** removed stale Expo 57/RN 0.86 deployment text,
  corrected the Qt 6.11.1 CMake message, documented the hardware shopping list,
  and changed Hermes/Nitro/Expo status from "not implemented" to
  "implemented; physical target validation pending."
- **Remaining work:** requires the licensed SDP/current QSTI physical target:
  Screen run first, then Hermes, Nitro/Expo target proof, then the full
  React/Fabric/Qt Quick target runtime.

## 2026-10-03 — braces depth-exhaustion backport

- Investigated current main 8d521ea7b599f58288ebde76558c9b2d48622014 and the
  dependency-audit failure; no fixed npm release or duplicate open PR existed.
- Backported only PR #72 library diffs, pinned to commit
  28d440b5dd449dbf1fe6f3506cf94ecca4d02660, onto published braces 3.0.3.
- Added exact installed-source attestation, depth/compatibility regression tests,
  and a fail-closed audit classification that leaves the raw finding visible.
- Regenerated the lockfile with pnpm 12.8.1; it also records the four existing
  workspace manifests omitted from the prior lockfile, without dependency bumps.
- Local JS, typecheck, dependency/source contracts, peer/build/architecture/codegen,
  formatting, production QNX Metro bundle (772 KB), and Linux C++ host (2/2 CTest)
  checks pass. CMake 3.31.10 was installed after the initial missing-tool failure.
  Remote CI remains pending. No publish, merge, QNX build, or target run.
- Continue with exact-head CI and eventual removal after an official fixed release.

## 2026-11-10 — PR #111 CI coverage correction

Reviewed the actual hosted renderer workflow and found that both existing lanes
left the new SVG/motion/graphics capabilities OFF. Local native receipts did not
make those CI runs feature validation. Added an explicit enabled lane while
retaining OFF coverage, deterministic SVG generation and inventory guards for
all five native feature tests. The original validator fails the new behavioral
regression; the corrected implementation passes it.

Published implementation: `295f7240099c7371dc8988f4d411b1840888a415` (same tree
as local review commit `0cdc788f1e83d7beec10602d36558b6e0daf72c8`). Frozen install,
typecheck, SVG codegen, formatting and docs checks pass. The new hosted native
matrix is pending; no new device or release qualification is claimed. Applied
the repository validation skill and pinned Callstack `github-actions` guidance;
exact skill identities remain in the v2 receipt. Implementation/review owner:
agent `audit_followups`; human merge and target qualification remain unassigned.
