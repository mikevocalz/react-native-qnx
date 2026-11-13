# Dependency compatibility matrix

The exact direct pins come from `package.json`. Transitive versions and Hermes
source identity come from the installed frozen dependency graph. Regenerate
with `pnpm docs:sync`; check with `pnpm validate:docs`.

<!-- generated:dependencies:start -->

| Component              | Pinned / resolved version               |
| ---------------------- | --------------------------------------- |
| reactNative            | `0.88.1`                                |
| react                  | `19.3.0`                                |
| expo                   | `58.0.24`                               |
| metro                  | `0.87.1`                                |
| reactNativeMetroConfig | `0.88.1`                                |
| babelPresetExpo        | `58.0.25`                               |
| nitroCore              | `1.0.0-alpha.nightly.136.gdb81de61bd32` |
| nitroModules           | `1.0.0-alpha.nightly.136.gdb81de61bd32` |
| nitrogen               | `1.0.0-alpha.nightly.136.gdb81de61bd32` |
| typescript             | `7.0.2`                                 |
| codegenTypescript      | `6.0.2`                                 |
| node                   | `>=26.0.0`                              |
| packageManager         | `pnpm@12.18.0`                          |
| expoModulesCore        | `58.1.0`                                |
| expoMetro              | `58.0.0`                                |
| expoMetroConfig        | `58.0.22`                               |
| hermesCompiler         | `260318099.0.5`                         |
| hermesSource           | `hermes-v260318099.0.5`                 |

<!-- generated:dependencies:end -->

### Opt-in third-party packages

These install from the workspace lockfile but only take effect when their
default-OFF native switch is on. A row is not a support claim beyond the
evidence it links.

| Package            | Pinned version | Switch                     | Host (Linux CI)                                                            | Pi 400 device                                                                         | Not supported                                                                                                               |
| ------------------ | -------------- | -------------------------- | -------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `react-native-svg` | `15.16.0`      | `RNQNX_ENABLE_QT_RNSVG=ON` | Fabric fixture with the unmodified package and golden-pixel renderer tests | Scoped: gallery Advanced tab, see [the receipt](../v2/validation.md#react-native-svg) | Pattern, Symbol, Marker, TextPath, Filter/Fe\*, ForeignObject; see [the element table](../v2/react-native-svg.md#supported) |

`15.16.0` is the version Expo SDK 58 lists in `bundledNativeModules.json`.

### Held: react 19.3.1

`react` stays at `19.3.0` until the owner decides. React 19.3.1 is published,
but `react-native` 0.88.1 ships its Fabric renderer prebuilt from React
`19.3.0` (`version` and `reconcilerVersion` in
`Libraries/Renderer/implementations/ReactFabric-*.js`). React requires the
`react` package and the renderer to be the same version, so bumping `react`
alone would pair a 19.3.1 package with a 19.3.0 renderer. The bump waits for a
React Native release that bundles a 19.3.1 renderer, or for an owner decision
to carry a mismatched pair.

### Held: metro 0.88.0

`metro` stays at `0.87.1`. `@react-native/community-cli-plugin` 0.88.1 depends
on `metro` `^0.87.1`, and `@react-native/metro-config` 0.88.1 on
`metro-config` and `metro-runtime` `^0.87.1`. A caret on a `0.x` version
excludes the next minor, so 0.88.0 is outside the range React Native 0.88.1
supports. The bump waits for a React Native release that accepts Metro 0.88.

These versions do not imply upstream QNX support. The platform supplies the
QNX integration and keeps the exact Nitro v1 alpha pin. The constrained
platform-info generator is not general Nitrogen QNX support. See
[the Nitro policy](../platform-modernization/NITRO_V1_API_POLICY.md).

## Expo native modules on QNX

Expo modules run through the QNX Expo host in
`packages/expo-modules-core-qnx`. The host installs `globalThis.expo` before
the bundle runs. Metro swaps expo-modules-core's polyfill entry for
`packages/react-native-qnx/expo/polyfill.js`, which supplies the JavaScript
classes (`EventEmitter`, `NativeModule`, `SharedObject`, `SharedRef`) that iOS
and Android define natively. A module reaches an app when a direct dependency
declares `"react-native-qnx": {"expo": {...}}` in its package.json, the same
shape as a Nitro module (docs/adr/0019). `project prepare-native` records it as
`expoModules` in `native-app.json`, and `RnqnxConsumerApp.cmake` links it and
calls its initializer before `globalThis.expo` is installed.

| Package                                 | QNX status    | Implementation or reason                                                                                                                                                                                                                                                                                                      |
| --------------------------------------- | ------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `expo-modules-core` 58.1.0              | works         | QNX host: `modules`, `uuidv4`, `uuidv5`, `getViewConfig` (returns null), `expoModulesCoreVersion`; JS classes from the QNX polyfill                                                                                                                                                                                           |
| `expo-constants` 58.0.16                | works         | `@rn-qnx/expo-constants-qnx`: embedded app config, `uname` device and system fields, `executionEnvironment: bare`                                                                                                                                                                                                             |
| `expo-router` 58.0.31                   | works (JS)    | Runs unchanged through `@rn-qnx/expo-router-qnx` (Metro resolver). `Tabs` and `Drawer` render through the native QtTabBar and QtDrawer when the host has them, else from React Native views. Not supported: native-tabs, native-stack screens (react-native-screens), `expo-router/ui` web tabs, API routes, server rendering |
| `expo-linking` 58.0.17                  | works (JS)    | `ExpoLinking` is a JavaScript stub: no launch URL, no URL events. `Linking.openURL` rejects on QNX                                                                                                                                                                                                                            |
| `expo-status-bar` 58.0.5                | pinned        | expo-router depends on it. Nothing in the gallery renders it, and `StatusBar` has no effect on a QNX window                                                                                                                                                                                                                   |
| `react-native-safe-area-context` 5.11.1 | replaced      | Metro substitutes a JavaScript module with zero insets; the host's title bar sits outside the React surface                                                                                                                                                                                                                   |
| `react-native-screens` 4.30.1           | replaced      | Metro substitutes a module whose `screensEnabled()` is false; navigators use plain Views, and inactive screens stay mounted                                                                                                                                                                                                   |
| `react-native-drawer-layout` 4.2.12     | replaced      | Metro substitutes the QtDrawer adapter (no Reanimated or gesture-handler)                                                                                                                                                                                                                                                     |
| `@expo/metro-runtime` 58.0.17           | not used      | Pinned because expo-router peers on it; the QNX entry does not import it                                                                                                                                                                                                                                                      |
| `expo-image` 58.0.18                    | partial       | `@rn-qnx/expo-image-qnx`: the `ExpoImage` module and view config; the view is the `ViewManagerAdapter_ExpoImage` Fabric component on the shared image loader. Limits: [the package README](../../packages/expo-image-qnx/README.md)                                                                                           |
| `expo-application`                      | not ported    | Needs install time, build identifiers and an install ID store on QNX; no implementation yet                                                                                                                                                                                                                                   |
| `expo-updates`                          | not supported | Replaces the embedded bundle at run time; the QNX consumer build pins the bundle hash in CMake and has no update channel                                                                                                                                                                                                      |
| `expo-dev-client`                       | not supported | iOS/Android launcher UI; QNX has its own development client (RNQNX_ENABLE_DEV_SUPPORT, see dev-workflow.md)                                                                                                                                                                                                                   |
| `expo-notifications`                    | not supported | Needs APNs/FCM; QNX has no push service                                                                                                                                                                                                                                                                                       |
| `expo-camera`                           | not supported | Needs a camera pipeline into Qt Quick; the Pi 400 image has no camera stack                                                                                                                                                                                                                                                   |
| `expo-location`                         | not supported | No location service on the QNX image                                                                                                                                                                                                                                                                                          |
| `expo-sensors`                          | not supported | No sensor framework on the Pi 400 image                                                                                                                                                                                                                                                                                       |
| `expo-secure-store`                     | not supported | Needs a keychain or keystore; QNX has no equivalent service in this image                                                                                                                                                                                                                                                     |
| `expo-web-browser`                      | not supported | No system browser or WebView on QNX                                                                                                                                                                                                                                                                                           |
| `expo-glass-effect`                     | not supported | iOS 26 Liquid Glass API only                                                                                                                                                                                                                                                                                                  |
| `expo-reload`/`reloadAppAsync`          | not supported | `globalThis.expo.reloadAppAsync` throws; reload from the development client's menu                                                                                                                                                                                                                                            |

Modules not in this table have not been checked on QNX.

## Target and toolchain

| Target               | Requirement                                           | Evidence boundary                                                                      |
| -------------------- | ----------------------------------------------------- | -------------------------------------------------------------------------------------- |
| Development host     | Qt ≥6.11.1, CMake ≥3.31.9, Ninja ≥1.11, Python ≥3.11  | Host tests and offscreen captures                                                      |
| QNX cross-build host | Licensed SDP 8.0.5 on a supported x86-64 host         | Compilation is separate from device execution                                          |
| Stock Pi 4/5         | QSTI 0.5.0 build 50                                   | Full runtime and device parity still need receipts                                     |
| Experimental Pi 400  | Custom BCM2711 image; Qt 6.12.0 evaluation; QNX 8.0.5 | [Hello World](../evidence/pi400-hello-world-2026-11-07.txt), not stock QSTI validation |

The Pi 400 runtime uses the restricted Qt module subset in
[ADR-0011](../adr/0011-qt-module-licensing.md). Evaluation evidence does not
resolve production distribution. Source/archive/plugin identities remain in
[the Pi 400 guide](pi400-hello-world.md).

## Check a change

```bash
pnpm install --frozen-lockfile
pnpm validate:dependencies
pnpm typecheck
pnpm test:js
pnpm validate:docs
```

Dependency upgrades require a regenerated lockfile, source-contract checks,
codegen checks and the affected native graph. Cross-build and named target runs
remain separate gates. The old Hermes patch is historical; use the current
source tag and `scripts/patch-hermes-qnx.mjs`.

The registry audit runs separately with `pnpm audit:dependencies`. The
[braces backport](../security/braces.md) is a source-verified mitigation,
not a claim that npm's registry report is empty.
