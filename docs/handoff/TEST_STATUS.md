# TEST_STATUS.md

## Shadow tree to Qt items, end to end — 2026-08-14

- CTest is 7/7. New: `shadow_tree_mounting_host_test`, five cases.
- The pipeline under test is entirely React Native's: a real `ShadowTree` is
  committed from C++, React Native's differ computes the mutations,
  `MountingCoordinator::pullTransaction` hands over a `MountingTransaction`, and
  the adapter applies it to Qt Quick items. The geometry asserted is Yoga's,
  computed by React Native, not by this repository.
- Cases: a fixed 120x60 view reaches the item; two flex children split a 160pt
  column 1:3 with correct stacking offsets; 16pt padding offsets a child's
  origin; a second commit resizes the same item in place rather than replacing
  it; removing a child from the tree unmounts its item.
- Finding: view flattening. The first run produced exactly one mutation, an
  Update of the root sized to the child, and no Create — React Native flattens
  views carrying only layout props. The test sets `collapsable = false`, and the
  behaviour is documented in the cited-seams ledger rather than worked around.
- Mutation check: removing `setY` from the mounting layer failed
  `flexChildrenSplitTheRootWidth` (`0 != 40`) and `paddingOffsetsChildOrigin`
  (`0 != 16`). Reverted and re-run green.
- Evidence level: host only. No JavaScript runs — the tree is committed from
  C++ — and nothing has been cross-compiled for QNX or run on the Pi 4.

## React Native renderer builds from source — 2026-08-14

- 41 React Native 0.86.2 modules now build on the macOS host by adding React
  Native's own per-module `CMakeLists.txt` files: the renderer core, graphics,
  mounting, componentregistry, view/root/scrollview/text components,
  attributedstring, textlayoutmanager, uimanager, runtimescheduler, css,
  consistency, observers, performance timeline, jsinspector, cxxreact, Yoga,
  JSI, and their support modules.
- Third-party is now compiled rather than header-only: `folly_runtime` builds
  all 29 sources React Native lists, plus real glog, fmt, and double-conversion
  libraries and React Native's trimmed boost headers.
- `rnqnx_fabric_adapter` links `react_renderer_mounting` instead of compiling
  `ShadowViewMutation.cpp` directly.
- CTest remains 6/6 with every existing case passing against the real modules.
- Five problems found and fixed during bring-up, each by using React Native's
  own mechanism rather than working around it: the platform selector had to be
  included for the generic `cxx` paths; `REACT_ANDROID_DIR` had to be set so
  `target_merge_so` resolves (it is a no-op off Android); nine `platform/cxx`
  include directories had to be added globally, matching React Native's
  assembler; Yoga's target is `yogacore` and needed the `yoga` alias; and OBJECT
  library objects had to be attached explicitly because they are not archived
  into a static library that links them.
- Evidence level: host-compiled and host-linked. None of these modules has been
  cross-compiled with `qcc` or run on the Pi 4, and no bundle has been executed.

## React Native headers and mutation adapter — 2026-08-14

- CTest is 6/6. New: `rnqnx_rn_headers_probe` and
  `shadow_view_mutation_adapter_host_test` (seven internal cases).
- React Native 0.86.2 renderer headers compile here for the first time, against
  fetched folly 2024.11.18.00, fmt 12.1.0, glog 0.3.5, double-conversion 1.1.6,
  and fast_float 8.0.0. Boost is not needed and is not fetched.
- `rnqnx_rn_headers_probe` enforces every mirror in the cited-seams ledger with
  `static_assert`, including the five `ShadowViewMutation::Type` values and the
  identity of `Tag` and the layout `Float`.
- The adapter applies real `ShadowViewMutation` values built through React
  Native's own static factories: create/insert with initial layout, index
  ordering, update, remove-then-delete, nested trees, and reported (not skipped)
  failures for unmapped components and missing parents.
- Two problems found and fixed during bring-up: glog defines a `CHECK` macro
  that collided with the test harness, renamed to `EXPECT`; and
  `RN_DEBUG_STRING_CONVERTIBLE` pulled `ShadowView.cpp`'s debug-string overloads
  into the link, resolved with React Native's own `REACT_NATIVE_PRODUCTION`
  switch rather than by stubbing the symbols.
- Mutation check: forcing the adapter's Insert to always use index 0 made
  `insertOrderFollowsMutationIndex` fail at the expected assertion. Reverted and
  re-run green.
- Evidence level: host only. Transactions are constructed in tests, not pulled
  from a live surface, and nothing has been cross-compiled for QNX.

## Fabric mounting layer host tests — 2026-08-14

- CTest is 4/4: `rnqnx_g0_cxx20_probe`, `qnx_screen_surface_host_test`,
  `qtquick_mounting_layer_host_test`, `rnqnx_qtquick_host_probe`.
- `qtquick_mounting_layer_host_test` reports fifteen internal cases, all passing
  under strict warnings, ASan, and non-recovering UBSan on the offscreen QPA
  plugin.
- Coverage: root borrowed and undeletable; create leaves items unparented;
  insert ordering including insert-at-0 ahead of existing children; unknown
  parent, unknown tag, and out-of-range index rejected; remove detaches without
  destroying; remove rejects index disagreement and wrong-parent claims; destroy
  releases both tag and item; destroy detaches a still-mounted item; a nested
  three-level tree is torn down with the layer; layout applies absolute
  geometry; unregistered type, broken QML source, non-item type, and duplicate
  tag each fail with the specific error; re-registering a type invalidates the
  cached component.
- Mutation check: forcing the restacking branch off made
  `insertParentsAndOrdersChildren` fail at the expected assertion, so the
  ordering coverage is not vacuous. The change was reverted and the suite
  re-run green.
- Evidence level: host only. No React Native header is compiled here, no QNX
  translation unit exists, and nothing has run on the Pi 4.

## Qt Quick host bring-up — 2026-08-14

- Host: macOS 26.6.1, Darwin 25.6.0 arm64, Apple Clang 21.0.0.21000099,
  Homebrew Qt 6.11.1 at `/opt/homebrew/opt/qt`.
- `find_package(Qt6 6.8 COMPONENTS Core Gui Qml Quick QUIET)` resolves with no
  `CMAKE_PREFIX_PATH` hint on this machine; CMake logged
  `Qt 6.11.1 found; building Qt Quick host targets`.
- `rnqnx_views_qt` and `rnqnx_qtquick_host_probe` compile clean under
  `-Wall -Wextra -Wpedantic -Werror` against public Qt headers only.
- CTest is 3/3: `rnqnx_g0_cxx20_probe`, `qnx_screen_surface_host_test`, and
  `rnqnx_qtquick_host_probe` (offscreen QPA).
- Probe output:

  ```text
  RNQNX_QTQUICK_HOST_OK items=1000 create_ms=12.856 update10_ms=0.456 destroy_ms=0.516 platform=offscreen
  ```

- Evidence level: host-compiled, host-executed. This is not Qt-on-QNX evidence.
  No Qt code has been cross-compiled or run on the Pi 4, the QNX QPA plugin over
  Screen is unbuilt, and G0-B and G0-C remain open. Nothing here promotes a QNX
  validation row.

## Latest host validation — 2026-08-14

Re-ran the full host suite before committing the batch.

- Host: macOS 26.6.1, Darwin 25.6.0 arm64, Apple Clang 21.0.0.21000099.
- `pnpm format:check` failed first on `docs/cited-seams.md` and
  `docs/getting-started/compatibility-matrix.md`; `pnpm format` fixed both and
  the re-run passed.
- `pnpm validate:macos` then completed all eight stages with exit 0.
- CTest is now 2/2, not 1/1: `rnqnx_g0_cxx20_probe` (0.27 s) and
  `qnx_screen_surface_host_test` (0.38 s, eight internal cases). The probe
  passing here is host evidence only — G0-A requires that binary to run on the
  Pi 4, which has not happened.
- `pnpm install --frozen-lockfile` reported "Already up to date".
- Not-applicable stages are unchanged: no `tests/js` files, no package
  manifests, no `nitro.json`.

The dated table below remains the detailed per-check record; only the CTest
count and the two Prettier files changed.

## Host validation — 2026-08-13

- Host: macOS 26.6.1, Darwin 25.6.0 arm64, Apple Clang 21.0.0.
- Repository package manager: pnpm 11.21.0.

| Check                                 | Result         | Evidence                                                                                       |
| ------------------------------------- | -------------- | ---------------------------------------------------------------------------------------------- |
| `pnpm validate:macos`                 | Passed         | All eight host stages completed with exit 0.                                                   |
| Prettier                              | Passed         | All supported files match Prettier 3.9.6.                                                      |
| TypeScript                            | Passed         | TypeScript 6.0.3 `tsc --noEmit`, strict JS checking enabled.                                   |
| Installed dependency/source contracts | Passed         | Exact pins, Expo graph, Hermes tag, and 19 named installed-source checks passed.               |
| Scoped `xcode>uuid` compatibility     | Passed         | uuid 11.1.1 resolves; xcode's call site and CommonJS `uuid.v4` export are present.             |
| `pnpm audit --audit-level moderate`   | Failed         | Two high `image-size` DoS advisories remain; no fixed registry release exists.                 |
| JavaScript tests                      | Not applicable | `tests/js` contains no test files; the runner reported an explicit skip.                       |
| QNX Screen C++ host target            | Passed         | Strict C++20, ASan, and non-recovering UBSan; CTest 1/1 passed.                                |
| Internal Screen wrapper cases         | Passed         | Eight cases cover setup, ARGB fill, buffer refresh, cleanup, moves, and init/runtime failures. |
| Workspace package build               | Not applicable | No app/package manifests exist yet; 15 directories are intentionally scaffold-only.            |
| Manifest validation                   | Passed         | 0 manifests and 15 scaffold-only directories reported accurately.                              |
| Architecture validation               | Passed         | Required tracked scaffold directories and production files are present.                        |
| Nitrogen generated-output check       | Not applicable | No `nitro.json` exists; QNX generation has not been designed.                                  |
| `git diff --check`                    | Passed         | No whitespace errors.                                                                          |

The CTest count is one executable; that executable reports eight independent
test cases. Host tests compile against `tests/cpp/qnx-screen-fake`, never an
installed QNX header or `libscreen`.

## Expected unsupported-host diagnostic — 2026-08-13

`bash scripts/configure-qnx.sh aarch64 Debug` exited 1 on macOS as expected. The
primary CMake error was:

```text
Environment variable QNX_HOST is not set.
```

CMake also emitted downstream compiler/generator errors after the toolchain
fatal error. This is not a QNX build attempt and is not a pass.

`bash scripts/doctor.sh` also exited 1 as expected: eight QNX environment errors
and five host/target warnings. Its CMake, Ninja, Node, exact pnpm, Python, and
installed dependency/source checks all passed.

## QNX validation status

| Suite                                      | Status  | Reason                                                                     |
| ------------------------------------------ | ------- | -------------------------------------------------------------------------- |
| Installed QNX `screen/screen.h` audit      | Not run | QNX SDP 8.0.5 is not installed in this workspace.                          |
| QNX AArch64/x86-64 cross-compilation       | Not run | No `QNX_HOST`, `QNX_TARGET`, `qcc`, `q++`, or licensed sysroot.            |
| `libscreen` linkage                        | Not run | QNX Screen development library is unavailable.                             |
| Virtual/physical QNX target runtime        | Not run | No QNX target is configured.                                               |
| Target resource, presentation, and cleanup | Not run | Requires the installed Screen service, BSP/driver, compositor, and target. |

Do not promote any QNX queue state from host evidence. The exact continuation is
recorded in `QNX_VALIDATION_QUEUE.md`.

## Dependency-audit exception

The original audit also reported vulnerable uuid 7.0.3 through xcode. A scoped
override to uuid 11.1.1 removed that moderate advisory, and its installed call
shape is part of `validate:dependencies`. The two remaining high findings are
`UPSTREAM-DEP-001`; they are recorded rather than ignored. Consequently, “full
host validation passed” does not mean “registry security audit is clean.”
