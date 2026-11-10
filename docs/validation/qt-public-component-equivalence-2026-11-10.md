# Public React Native components and Qt package entries — 2026-11-10

## Scope and source boundary

Base: `0c91be9bbb4e7cd6dabccf8cdfe441c7fa57c629`. DRI: `/root/rn_qt_equivalence`;
the coordinating `/root` agent independently reviewed the additive exports,
resolver conditions and validation boundaries. Work is isolated from the active native motion/SVG
branch and from the separate native package-discovery workstream.

This change exposes existing implementations through the opt-in public React
Native entry, and enables installed packages to select their explicit Qt
JavaScript entry through Metro. It changes no renderer, native API, ABI,
dependency pin, native registration, permission, boot configuration or licensing
basis. Yoga, Fabric, Hermes and the existing Qt Quick backend remain in place.
No premium package, source, asset or binary is required by core tests or bundled
into the platform.

The public entry grows from 16 to 28 lazy exports. The 12 additions are `Button`,
`TouchableOpacity`, `ImageBackground`, `VirtualizedList`, `Dimensions`,
`useWindowDimensions`, `PixelRatio`, `I18nManager`, `Easing`, `PanResponder`,
`processColor` and `findNodeHandle`. They use direct upstream imports; the
existing target resolver still selects the platform overrides where present.
No wrapper changes an upstream method's argument, return or error contract.

`I18nManager` deliberately exposes the installed RN implementation. Its optional
native module is absent on this baseline, so RN returns `isRTL: false` and
`doLeftAndRightSwapInRTL: true`; its configuration methods cannot change native
layout direction. The fallback is tested and documented, not promoted into a
native RTL implementation. Existing core components already use this upstream
fallback internally.

The QNX and Marathon Metro factories append their own target condition under
`resolver.unstable_conditionsByPlatform`. Global React Native conditions,
custom conditions and other platforms are preserved. A package must place
`qnx` or `marathon` before `react-native` in each applicable conditional export.
This addresses the case where an explicit `.native.js` export otherwise bypasses
platform suffix resolution. It does not prove that a selected module's peer
dependencies or native registrations are available.

## Source and skill decisions

| Source                                                                     | Exact version or snapshot                   | Applied decision                                                                                                                   |
| -------------------------------------------------------------------------- | ------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| React Native and React                                                     | RN 0.88.1; React 19.3.0                     | Resolve the actual installed public component/helper implementation. Preserve lazy getters and explicit unsupported-export errors. |
| Metro                                                                      | 0.87.1; `@react-native/metro-config` 0.88.1 | Use per-platform export conditions, preserve existing configuration and test actual package root/subpath resolution.               |
| Expo preset                                                                | `babel-preset-expo` 58.0.23                 | Transform the installed upstream I18nManager for the JavaScript fallback test without changing its source.                         |
| Margelo `api-design`                                                       | `1e9e17be6f41c838db472d6a4b943fc26355a3be`  | Reuse existing APIs and semantics; do not disguise unavailable native services as new working providers.                           |
| Callstack `react-native-best-practices`, `bundle-barrel-exports` reference | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`  | Keep lazy imports; measure actual bundles and avoid inferred performance claims.                                                   |
| Repository `qnx-platform-validation`                                       | Base above                                  | Separate Node/Metro checks, basic native host tests, full Qt/Hermes runtime and physical targets.                                  |

The upstream skill files were read from local checkouts at the recorded commits.
This is source-reading evidence, not an installed skill CLI or a native runtime
compatibility claim. Nitro remains pinned to the existing v1 alpha105 runtime,
core and generator. This JavaScript slice does not implement or upgrade Nitro
bindings.

## Changed surfaces

- `packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js`: the 12 existing
  components and helpers become reachable from the public entry.
- `packages/metro-config-{qnx,marathon}/src/index.js` and declarations: target
  conditions are isolated, project settings remain intact, and the return type
  describes the complete config produced by merging Metro defaults.
- `apps/rn-playground/public-api-probe.js`: imports and renders the added
  composite controls and checks helper/DeviceInfo semantics when run in the host.
- JavaScript regressions: lazy export wiring, actual upstream fixed-LTR fallback,
  and a real installed-package conditional-export matrix.
- [Public entry guide](../getting-started/public-react-native-entry.md): exact
  export list, configuration examples and compatibility limits.

## Verification

Environment: Linux x86_64, Node 26.13.0, pnpm 12.8.1 and the repository's frozen
dependency graph. The C++ host preset uses GCC 14.2.0 and enables
`-fsanitize=address,undefined`. The source install used
`pnpm install --offline --frozen-lockfile --ignore-scripts`.

| Check                                                       | Observed result                           | What it establishes                                                                                                                                                                                                                  |
| ----------------------------------------------------------- | ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Public entry export test                                    | Pass                                      | 28 lazy getters; unsupported imports fail; opt-in resolution preserves other platforms. Synthetic require objects are used in this test.                                                                                             |
| Installed RN I18nManager test                               | Pass                                      | Actual RN fallback constants and unavailable-native mutation behavior. No native provider is fabricated.                                                                                                                             |
| Package export matrix                                       | 8/8 pass                                  | Each factory's target, iOS, Android and web are bundled by real Metro and executed in Node VM. Target root and subpath entries win, the mobile provider does not execute on Qt targets, and an RN-only subpath retains its fallback. |
| Public probe production bundles                             | QNX 703,156 bytes; Marathon 703,197 bytes | Both expanded import graphs bundle. The probe's native runtime assertions have not been executed in this slice.                                                                                                                      |
| Existing Marathon resolver test                             | Pass                                      | Existing platform identity, override and asset resolver contracts remain intact.                                                                                                                                                     |
| Existing external-project Metro test                        | Both targets pass                         | External source tree with installed workspace dependencies bundles; QNX 482,540 bytes and Marathon 482,566 bytes. This is not a registry publication test.                                                                           |
| Existing integrated-consumer test                           | Both targets pass                         | Fresh external app doctor, discovery and public Metro paths pass. Native registration and host rendering remain unverified in this run.                                                                                              |
| Dependency, typecheck, architecture and manifest validators | Pass                                      | Exact dependency/source contracts, declared JS/TS types, architecture boundary and package manifests remain valid.                                                                                                                   |
| Basic CMake/CTest host preset                               | 2/2 pass                                  | C++20 and fake-QNX-Screen host regressions with the preset's ASan/UBSan flags. Qt was not found by this basic preset and its Qt targets were skipped.                                                                                |
| Full JavaScript suite                                       | Blocked                                   | The suite reaches `marathon-development-launcher.test.mjs`; this environment rejects its temporary Unix socket with `listen EPERM`. It is not reported as a full-suite pass.                                                         |
| Full Qt/Hermes host render                                  | Not run                                   | No completed renderer artifact was available for this candidate during this slice.                                                                                                                                                   |
| QNX cross-build, VM and physical target                     | Not run                                   | No new target qualification is claimed.                                                                                                                                                                                              |
| Marathon/Wayland physical target                            | Not run                                   | JavaScript `Platform.OS` selection is not shell/device qualification.                                                                                                                                                                |

Commands, with the pinned Node/pnpm and CMake/Ninja tools on PATH:

```sh
node tests/js/qnx-public-entry.test.mjs
node tests/js/qnx-public-i18n.test.mjs
node tests/js/qt-package-exports.test.mjs
node tests/js/qnx-public-entry-metro.test.mjs
node tests/js/marathon-metro-platform.test.mjs
node tests/js/qnx-external-project-metro.test.mjs
node tests/js/qnx-integrated-consumer.test.mjs
pnpm typecheck
pnpm validate:dependencies
pnpm validate:architecture
pnpm validate:manifests
pnpm test:js
CMAKE_BUILD_PARALLEL_LEVEL=2 pnpm test:cpp:host
```

The package matrix executes only its own small JS fixtures in Node VM; RN
bootstrap is disabled for that fixture. It is package selection evidence, not
Hermes, Qt rendering, provider registration or input proof. Raw local logs are
under `build/qt-public-entry-validation/`; generated binaries and logs are not
part of the distributable package.

No frame-time, memory, startup, input-latency or performance improvement is
claimed. Bundle sizes are measured for this expanded probe only, with no baseline
size comparison or device budget qualification.

### Independent packed-control check

The coordinating review also extracted actual `@nyc-tron/core`, `@nyc-tron/ui`
and `@nyc-tron/expo-controls` candidate tarballs into a fresh consumer. Real
Metro 0.87.1 selected this exact patched public entry and the QNX/Marathon
conditions for every Expo-controls root/leaf import. Both bundles passed with
zero resolver warnings and no selected mobile Expo provider. This check used
Node 24.22.1 and the checkout's provisioned RN 0.88.1 peer graph; it was not a
registry installation or a native render.

The cross-repository source audit covers the exact 341-capability roster. With
these 28 exports it identifies 266 shared-source candidates, 17 rows requiring
missing RN APIs, 49 requiring additional providers and nine pinned external
implementations. All retain unverified target acceptance. The broader direct
source import audit still reports APIs outside the frozen roster that are not
implemented here, as described below.

### Combined installed-native package integration

The public slice is combined with the independently reviewed generic native
discovery/registration change from local commit
`eb2b9dbfc0a5366684387ebfa66c2f9e42078b1d`. It keeps product libraries as selected
application dependencies, rechecks installed metadata and exact Nitro runtime
requirements, and uses the distinct QNX or Marathon native installer. Runtime
copies reject symlink paths that could redirect patching outside the build.

The final packed Ignition archive
`27d1d99c29d313fe78a8b24f13185ba5fde96904127b04050c2e3db9a81a1351` passed strict
installation, unchanged Nitrogen regeneration, and actual Metro selection for
both targets. Its public host factory and direct Nitro entry ran all eleven
engine lifecycle scenarios through real Hermes, JSI and the Qt dispatcher.
Separate adapter binaries reported the correct actual native platform and
excluded the opposite platform class. This is Linux host native-engine
evidence; the renderer/device rows above remain unverified.

The five discovery/CMake/containment regressions, public export/I18nManager
checks and eight-case Metro condition matrix pass on the combined branch.
The first root rerun lacked CMake on PATH; rerunning with the existing CMake
toolchain passed without weakening the configuration tests. The native review
also passed 52 existing consumer-configuration checks and eight build-plan
checks. The pinned target Rust archive and SDK remain explicit build inputs.

The complete package, bundle and executable hashes are recorded in
[Forge's installed-package receipt](https://github.com/mikevocalz/nitro-forge/blob/codex/qt-native-integration/docs/native/evidence/ignition-qt-2026-11-10.json)
and its [scope and reproduction guide](https://github.com/mikevocalz/nitro-forge/blob/codex/qt-native-integration/docs/native/IGNITION_QT.md).

## Remaining compatibility work and rollback

The public export list and [component inventory](../component-support.md) are
different contracts. An existing export does not establish complete component
behavior. This change retains the existing limitations of window metrics,
multitouch, accessibility, text input, modal lifecycle and animation drivers.

Shared UI package source still needs the right peers and native services.
`AccessibilityInfo`, `BackHandler`, `Keyboard`, `Linking`, `Appearance` and
`useColorScheme` remain unavailable from the public entry. Worklets, Reanimated,
Gesture Handler, SVG and mobile-only Expo view providers need separately tested
integration. Native RTL configuration remains unsupported. A package's catalog
row or successful import cannot be counted as a working Qt component without
testing its actual selected dependency graph and behavior.

Next acceptance step: run both expanded public probe bundles in the existing
Qt/Hermes host, then exercise installed selected-package controls on each target
with input, resize, lifecycle and removal. Keep review/publication draft until
the required runtime evidence is attached; source and Node tests do not replace
it.

Rollback: remove the opt-in public entry wrapper to return to the prior import
surface; revert this additive change to remove the new target export conditions.
No dependency migration, application data migration or native ABI rollback is
needed. Selected optional packages remain independently removable.
