# Package contents audit, 2026-11-12, base `b51b49c`

`pnpm pack --pack-destination <temp>` (pnpm 12.8.1, macOS) ran in every
`packages/*` directory whose `package.json` is not `private`. Nothing was
published. The tarballs were extracted, listed and deleted afterwards.

## Packed

| Package                           | Bytes | Files | SHA-256 (first 16) |
| --------------------------------- | ----- | ----- | ------------------ |
| `@rn-qnx/expo-modules-core-qnx`   | 3484  | 11    | `78a983f3f6fe19aa` |
| `@rn-qnx/expo-qnx-autolinking`    | 3783  | 5     | `3b3b433a8561df82` |
| `@rn-qnx/expo-qnx-config-plugins` | 1493  | 4     | `6496a6c81e200bb9` |
| `@rn-qnx/metro-config-marathon`   | 2253  | 4     | `d396c964dc3ca205` |
| `@rn-qnx/metro-config-qnx`        | 3098  | 6     | `e8c78fa4ae189ef3` |
| `@rn-qnx/qnx-nitro-runtime`       | 4319  | 15    | `293b406936da47f6` |
| `@rn-qnx/react-native-marathon`   | 1922  | 4     | `da6ab30debdbce7b` |
| `@rn-qnx/react-native-qnx`        | 7139  | 14    | `151923977bf894ee` |

Not packed: `@rn-qnx/react-native-qnx-cli` is `private`. `packages/expo-qnx`,
`packages/qnx-devtools` and `packages/react-native-qnx-codegen` have no
`package.json`; the CLI imports `qnx-devtools` by relative path, which only
works inside the checkout.

## Contents

- `expo-modules-core-qnx`: `LICENSE`, `CMakeLists.txt`, `index.js`, `index.d.ts`,
  `package.json`, `cpp/ExpoQnx{Installer,ModuleRegistry,ModulesHostObject}.{cpp,hpp}`.
- `expo-qnx-autolinking`: `LICENSE`, `bin/expo-qnx-autolinking.js`, `src/index.js`,
  `src/index.d.ts`, `package.json`.
- `expo-qnx-config-plugins`: `LICENSE`, `index.js`, `index.d.ts`, `package.json`.
- `metro-config-marathon`: `LICENSE`, `src/index.js`, `src/index.d.ts`, `package.json`.
- `metro-config-qnx`: `LICENSE`, `src/index.js`, `src/index.d.ts`, `public-entry.js`,
  `public-entry.d.ts`, `package.json`.
- `qnx-nitro-runtime`: `LICENSE`, `CMakeLists.txt`, `index.js`, `index.d.ts`,
  `package.json`, `cpp/` (`NitroLogger.cpp`, `QnxNitroRuntime.{cpp,hpp}`,
  `QnxPlatformInfo.{cpp,hpp}`, `generated/QnxPlatformInfo.binding.cpp`,
  `QtNitroDispatcher.{cpp,hpp}`, `ThreadUtils.cpp`, `NitroCoreCompat.hpp`).
- `react-native-marathon`: `LICENSE`, `index.js`, `Libraries/Utilities/Platform.js`,
  `package.json`.
- `react-native-qnx`: `LICENSE`, `index.js`, `package.json` and 11 files under
  `Libraries/` (AppState, Image, ImageBackground, KeyboardAvoidingView,
  BaseViewConfig, PublicAPI, RefreshControl, StatusBar, Switch, TextInput,
  Platform).

## Checks

| Check                                                       | Result                                                                                      |
| ----------------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| QNX SDP headers, libraries or host tools                    | none                                                                                        |
| Qt binaries, plugins or evaluation artifacts                | none                                                                                        |
| Nitro Forge or other premium component sources/assets       | none                                                                                        |
| Build outputs (`build/`, `*.o`, `*.a`, `*.so`, bundles)     | none                                                                                        |
| Secrets (keys, tokens, passwords, absolute `/Users/` paths) | none; the only grep hits are `parseArgs` `tokens` and a TextInput comment about edit tokens |
| `LICENSE` in every tarball                                  | yes                                                                                         |
| Relative `require()` targets present in the tarball         | yes, all eight                                                                              |
| `workspace:` ranges left in packed manifests                | none; rewritten to `0.1.0`                                                                  |

## Problems to fix before any publish

1. **`@rn-qnx/qnx-nitro-runtime` cannot configure from an install.** Its
   `CMakeLists.txt` runs
   `include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/RnqnxNitroV1.cmake")`, a path
   outside the package. Installed under `node_modules/@rn-qnx/qnx-nitro-runtime`,
   that resolves to `node_modules/cmake/RnqnxNitroV1.cmake`, which does not exist.
   Either ship the module inside the package or document the package as
   checkout-only. Not fixed here: native build wiring is outside this gate's
   scope.
2. **No `repository` field** in any manifest. npm provenance and the package
   page link both need it.
3. **Scope ownership is unverified.** `npm view @rn-qnx/react-native-qnx`
   returned a 404 on 2026-11-12. That shows the package is not published; it
   does not show who owns, or could register, the `@rn-qnx` scope.
   `docs/packaging.md` already marks the scope provisional. Owner action: create
   or confirm the npm organization, then set provenance, 2FA and CODEOWNERS.
4. Several packages import deep `react-native/Libraries/...` paths. They are
   pinned to `react-native@0.88.1` as a peer, so this holds for the reviewed
   graph, but a consumer on another React Native version will break at bundle
   time without a clear message.
