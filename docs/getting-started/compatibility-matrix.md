# Dependency Compatibility Matrix

This matrix records the active dependency and target baseline for
`react-native-qnx`. Direct workspace dependencies are exact pins in
`package.json` and `pnpm-lock.yaml`. Resolved entries are selected by that
lockfile. Bundled entries are source snapshots shipped by React Native.

Nothing in this matrix implies upstream QNX support. QNX compatibility requires
separate cross-build and target evidence.

## Registry audit snapshot — 2026-10-01

Expo SDK 58 is the selected product line. The published `expo@58.0.0` package
currently uses the npm `next` dist-tag rather than `latest`, but its own
manifest tests React 19.3.0 with React Native 0.88.0-rc.3. The repository
therefore follows that SDK contract and keeps the remaining direct tooling on
current published versions.

| Component                    | Selected    | Status / reason                                                             |
| ---------------------------- | ----------- | --------------------------------------------------------------------------- |
| Expo                         | 58.0.0      | SDK 58 product baseline                                                     |
| React Native                 | 0.88.0-rc.3 | Expo 58 tested development baseline                                         |
| React                        | 19.3.0      | Expo 58 tested development baseline                                         |
| Metro                        | 0.87.1      | Current Metro release resolved coherently with the selected RN Metro config |
| `@react-native/metro-config` | 0.88.0-rc.3 | RN 0.88 support-package line used by SDK 58 tooling                         |
| `babel-preset-expo`          | 58.0.4      | Current SDK 58 Babel preset                                                 |
| Nitro Modules                | 0.37.1      | Current published release                                                   |
| Nitrogen                     | 0.37.1      | Matches Nitro Modules                                                       |
| TypeScript                   | 7.0.2       | Current published release                                                   |
| `@types/react`               | 19.3.0      | Current React 19.3 typings                                                  |
| `@types/node`                | 26.6.3      | Current Node typings                                                        |
| Prettier                     | 3.9.9       | Current published release                                                   |
| pnpm                         | 12.8.1      | Current package-manager pin                                                 |
| Node.js                      | 26+         | Runtime floor aligned with current Node 26 typings                          |

## Locked Expo graph

The SDK-58 lockfile regenerated from npm resolves:

| Component            | Locked version          | Scope                                                                                 |
| -------------------- | ----------------------- | ------------------------------------------------------------------------------------- |
| `expo-modules-core`  | 58.0.9                  | Expo native module core; source contracts are audited, QNX implementation is separate |
| `@expo/metro`        | 58.0.0-rc.0             | Expo Metro fork used by SDK 58                                                        |
| `@expo/metro-config` | 58.0.6                  | Expo Metro defaults used for source-contract comparison                               |
| Hermes compiler      | 260318099.0.4           | React Native 0.88 compiler package                                                    |
| Hermes source tag    | `hermes-v260318099.0.2` | Source snapshot recorded by React Native                                              |
| `xcode > uuid`       | 11.1.1                  | Scoped override; validator checks `uuid.v4()` compatibility                           |

## Platform and toolchain

| Component                 | Status                       | Version / requirement       | Notes                                                   |
| ------------------------- | ---------------------------- | --------------------------- | ------------------------------------------------------- |
| macOS host                | Authoring / host validation  | current macOS + Apple Clang | Does not provide QNX runtime proof                      |
| Linux host                | Authoring / host validation  | current x86-64 Linux        | Runs non-QNX host checks                                |
| Windows x86-64 QNX host   | Historical proven build host | QNX SDP 8.0.2               | Existing AArch64 cross-build evidence was captured here |
| Current Pi build baseline | Target                       | QNX SDP 8.0.5               | Required for the current September 2026 QSTI lane       |
| Raspberry Pi QSTI         | Target image                 | 0.5.0 build 50              | Current Pi 4 / Pi 5 image baseline                      |
| CMake                     | Host prerequisite            | >= 3.31.9                   | Matches `cmake_minimum_required` and doctor             |
| Ninja                     | Host prerequisite            | >= 1.11                     | Checked by doctor                                       |
| Node.js                   | Host prerequisite            | >= 26.0.0                   | Aligns runtime with current Node typings                |
| pnpm                      | Exact pin                    | 12.8.1                      | Reproducible package-manager identity                   |
| Python                    | Host prerequisite            | >= 3.11                     | Used by native/tooling workflows                        |

## React Native runtime

| Component        | Status  | Version / identity      | Evidence and scope                                                       |
| ---------------- | ------- | ----------------------- | ------------------------------------------------------------------------ |
| React Native     | Pin     | 0.88.0-rc.3             | Out-of-tree QNX platform supplies the missing platform integration       |
| React            | Pin     | 19.3.0                  | Matches Expo 58                                                          |
| Yoga             | Bundled | RN 0.88 source snapshot | No standalone Yoga version is inferred                                   |
| Hermes           | Bundled | `hermes-v260318099.0.2` | QNX source rebase/tooling implemented; licensed target build/run pending |
| Fabric scheduler | Bundled | RN 0.88 source snapshot | Source symbols are checked by validator                                  |
| Fabric mounting  | Bundled | RN 0.88 source snapshot | Source symbols are checked by validator                                  |

The historical `patches/hermes-qnx-250829.patch` remains quarantined as
pre-SDK58 evidence. The current path uses the audited
`hermes-v260318099.0.2` source checkout plus
`scripts/patch-hermes-qnx.mjs`; source-contract CI is green, while the real
SDP 8.0.5 cross-build and Pi execution remain hardware gates.

## Expo, Metro, and TypeScript

| Component                    | Status   | Version         | Notes                                                                     |
| ---------------------------- | -------- | --------------- | ------------------------------------------------------------------------- |
| Expo / SDK                   | Pin      | 58.0.0 / SDK 58 | Published package; intentionally selected despite current `next` dist-tag |
| `babel-preset-expo`          | Pin      | 58.0.4          | Playground Babel preset                                                   |
| Metro                        | Pin      | 0.87.1          | QNX resolver extends the RN Metro defaults                                |
| `@react-native/metro-config` | Pin      | 0.88.0-rc.3     | Used by `@rn-qnx/metro-config-qnx`                                        |
| `expo-modules-core`          | Resolved | 58.0.9          | Android source is contract evidence only, not a QNX implementation        |
| `@expo/metro`                | Resolved | 58.0.0-rc.0     | SDK-58 Metro fork                                                         |
| `@expo/metro-config`         | Resolved | 58.0.6          | SDK-58 Expo Metro config                                                  |
| TypeScript                   | Pin      | 7.0.2           | Current published compiler                                                |

## Native interop

| Component                  | Status   | Version | Scope                                                                                                             |
| -------------------------- | -------- | ------- | ----------------------------------------------------------------------------------------------------------------- |
| React Native Nitro Modules | Pin      | 0.37.1  | Synchronous QNX runtime + `QnxPlatformInfo` HybridObject implemented; target run pending                          |
| Nitrogen                   | Pin      | 0.37.1  | Matches Nitro runtime; generated Apple/Android paths are not treated as QNX support                               |
| Expo Modules Core          | Resolved | 58.0.9  | QNX JSI registry/bootstrap implemented; package-specific QNX modules and target run pending                       |
| Expo QNX autolinking       | Repo     | 0.1.0   | QNX resolver/CMake generator implemented on top of Expo discovery; target-package implementations remain explicit |

## Graphics and text

| Component       | Status                    | Notes                                                                     |
| --------------- | ------------------------- | ------------------------------------------------------------------------- |
| Qt 6 / Qt Quick | Selected backend          | Host renderer uses Qt Quick; target Qt/QPA proof is still a QNX milestone |
| QNX Screen      | Target API                | Direct Screen smoke app is the physical-device baseline beneath Qt QPA    |
| EGL / GLES      | Target-dependent          | Checked from the installed QNX SDP/BSP                                    |
| Vulkan          | Future / target-dependent | Not required for the same-day Screen lane                                 |

## Validation rules

After installing dependencies:

```bash
pnpm install --frozen-lockfile
pnpm validate:dependencies
pnpm typecheck
pnpm test:js
```

The dependency validator checks exact direct pins, Expo 58's React/RN baseline,
resolved SDK-58 packages, Hermes identity, Nitro pairing, the uuid override, and
selected installed source symbols. These checks detect upstream drift but do not
replace compilation.

For the current Raspberry Pi path:

```bash
export QNX_SDP_ENV=/path/to/qnxsdp-env.sh
./scripts/pi-preflight.sh
./scripts/install-pi.sh
```

A passing Pi lane proves the QNX AArch64 toolchain -> deploy -> QNX Screen
runtime loop. It does not by itself prove the full React Native + Hermes + Qt
Quick stack on the target.

## Security audit

The first SDK-58 audit on 2026-10-01 exposed 23 advisory instances, all
collapsing to three transitive packages. Patched releases exist, so the
workspace scopes explicit overrides instead of accepting the vulnerable graph:

| Dependency path                | Vulnerable lock | Patched pin |
| ------------------------------ | --------------: | ----------: |
| `@expo/plist > @xmldom/xmldom` |          0.8.14 |      0.8.15 |
| `plist > @xmldom/xmldom`       |          0.9.11 |      0.9.12 |
| `@expo/xcpretty > js-yaml`     |           4.3.1 |       4.3.2 |
| `minimatch > brace-expansion`  |           5.0.9 |      5.0.12 |

These are checked by `scripts/validate-dependencies.mjs`; the audit must still
run in CI so a new advisory or upstream graph change cannot silently bypass the
pins. The existing `xcode > uuid` 11.1.1 override remains source-checked too.

## Updating this matrix

1. Select the requested Expo SDK.
2. Read Expo's published React/React Native baseline.
3. Query current direct package versions.
4. Regenerate the lockfile from npm.
5. Run dependency/source-contract validation.
6. Rebase source patches whose upstream identity changed.
7. Run host validation.
8. Cross-build with the licensed QNX toolchain.
9. Run the physical Pi evidence lane before claiming target support.
