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

| Component | Selected | Status / reason |
| --- | --- | --- |
| Expo | 58.0.0 | SDK 58 product baseline |
| React Native | 0.88.0-rc.3 | Expo 58 tested development baseline |
| React | 19.3.0 | Expo 58 tested development baseline |
| Metro | 0.87.1 | Current Metro release resolved coherently with the selected RN Metro config |
| `@react-native/metro-config` | 0.88.0-rc.1 | RN 0.88 support-package line used by SDK 58 tooling |
| `babel-preset-expo` | 58.0.4 | Current SDK 58 Babel preset |
| Nitro Modules | 0.37.1 | Current published release |
| Nitrogen | 0.37.1 | Matches Nitro Modules |
| TypeScript | 7.0.2 | Current published release |
| `@types/react` | 19.3.0 | Current React 19.3 typings |
| `@types/node` | 26.6.3 | Current Node typings |
| Prettier | 3.9.9 | Current published release |
| pnpm | 12.8.1 | Current package-manager pin |
| Node.js | 26+ | Runtime floor aligned with current Node 26 typings |

## Locked Expo graph

The SDK-58 lockfile regenerated from npm resolves:

| Component | Locked version | Scope |
| --- | --- | --- |
| `expo-modules-core` | 58.0.9 | Expo native module core; source contracts are audited, QNX implementation is separate |
| `@expo/metro` | 58.0.0-rc.0 | Expo Metro fork used by SDK 58 |
| `@expo/metro-config` | 58.0.6 | Expo Metro defaults used for source-contract comparison |
| Hermes compiler | 260318099.0.4 | React Native 0.88 bundled Hermes source identity |
| `xcode > uuid` | 11.1.1 | Scoped override; validator checks `uuid.v4()` compatibility |

## Platform and toolchain

| Component | Status | Version / requirement | Notes |
| --- | --- | --- | --- |
| macOS host | Authoring / host validation | current macOS + Apple Clang | Does not provide QNX runtime proof |
| Linux host | Authoring / host validation | current x86-64 Linux | Runs non-QNX host checks |
| Windows x86-64 QNX host | Historical proven build host | QNX SDP 8.0.2 | Existing AArch64 cross-build evidence was captured here |
| Current Pi build baseline | Target | QNX SDP 8.0.5 | Required for the current September 2026 QSTI lane |
| Raspberry Pi QSTI | Target image | 0.5.0 build 50 | Current Pi 4 / Pi 5 image baseline |
| CMake | Host prerequisite | >= 3.31.9 | Matches `cmake_minimum_required` and doctor |
| Ninja | Host prerequisite | >= 1.11 | Checked by doctor |
| Node.js | Host prerequisite | >= 26.0.0 | Aligns runtime with current Node typings |
| pnpm | Exact pin | 12.8.1 | Reproducible package-manager identity |
| Python | Host prerequisite | >= 3.11 | Used by native/tooling workflows |

## React Native runtime

| Component | Status | Version / identity | Evidence and scope |
| --- | --- | --- | --- |
| React Native | Pin | 0.88.0-rc.3 | Out-of-tree QNX platform supplies the missing platform integration |
| React | Pin | 19.3.0 | Matches Expo 58 |
| Yoga | Bundled | RN 0.88 source snapshot | No standalone Yoga version is inferred |
| Hermes | Bundled | `hermes-v260318099.0.4` | Source identity only; QNX port must be rebased and retested |
| Fabric scheduler | Bundled | RN 0.88 source snapshot | Source symbols are checked by validator |
| Fabric mounting | Bundled | RN 0.88 source snapshot | Source symbols are checked by validator |

The existing `patches/hermes-qnx-250829.patch` belongs to the prior
RN-0.86/Hermes-250829 source line. It is not valid SDK-58 evidence until rebased
against `260318099.0.4`.

## Expo, Metro, and TypeScript

| Component | Status | Version | Notes |
| --- | --- | --- | --- |
| Expo / SDK | Pin | 58.0.0 / SDK 58 | Published package; intentionally selected despite current `next` dist-tag |
| `babel-preset-expo` | Pin | 58.0.4 | Playground Babel preset |
| Metro | Pin | 0.87.1 | QNX resolver extends the RN Metro defaults |
| `@react-native/metro-config` | Pin | 0.88.0-rc.1 | Used by `@rn-qnx/metro-config-qnx` |
| `expo-modules-core` | Resolved | 58.0.9 | Android source is contract evidence only, not a QNX implementation |
| `@expo/metro` | Resolved | 58.0.0-rc.0 | SDK-58 Metro fork |
| `@expo/metro-config` | Resolved | 58.0.6 | SDK-58 Expo Metro config |
| TypeScript | Pin | 7.0.2 | Current published compiler |

## Native interop

| Component | Status | Version | Scope |
| --- | --- | --- | --- |
| React Native Nitro Modules | Pin | 0.37.1 | Registry/current source contract; QNX runtime still repository work |
| Nitrogen | Pin | 0.37.1 | Matches Nitro runtime |
| Expo Modules Core | Resolved | 58.0.9 | QNX package scaffolds exist; implementation remains separate |

## Graphics and text

| Component | Status | Notes |
| --- | --- | --- |
| Qt 6 / Qt Quick | Selected backend | Host renderer uses Qt Quick; target Qt/QPA proof is still a QNX milestone |
| QNX Screen | Target API | Direct Screen smoke app is the physical-device baseline beneath Qt QPA |
| EGL / GLES | Target-dependent | Checked from the installed QNX SDP/BSP |
| Vulkan | Future / target-dependent | Not required for the same-day Screen lane |

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

Do not carry forward the August 2026 dependency-audit result as if it applied to
this graph. Run `pnpm audit:dependencies` against the SDK-58 lockfile and
record any current findings in `docs/handoff/BLOCKERS.md`.

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
