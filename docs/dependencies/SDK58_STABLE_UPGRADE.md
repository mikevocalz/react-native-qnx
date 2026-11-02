# Dependency modernization: Expo 58 stable and React Native 0.88 stable

**DRAFT: package manifests updated, lockfile and native code compilation still blocked.** Date: 2026-11-02. This PR cannot be merged as-is.

## Exact npm observations (2026-11-02)

| Package | Prior baseline | Selected registry release | Why |
| --- | --- | --- | --- |
| expo | 58.0.0 | **58.0.22** (latest) | stable SDK 58 patch |
| react-native | 0.88.0-rc.3 | **0.88.1** (latest stable) | Expo 58 / RN0.88 release family |
| react | 19.3.0 | **19.3.0** | Expo 58 tested React family; unchanged |
| @react-native/metro-config | 0.88.0-rc.3 | **0.88.1** | matching RN patch |
| metro | 0.87.1 | **0.87.1 (SDK-compatible)** | Expo 58.0.22 pins @expo/metro to 0.87.1; Metro 0.87.2 is published but is not the tested graph |
| babel-preset-expo | 58.0.4 | **58.0.23** | Expo SDK58 patch |
| @types/node | 26.6.3 | **26.6.6** | patch upgrade |
| @types/react | 19.3.0 | **19.3.0** | unchanged |
| typescript | 7.0.2 | **7.0.2** | unchanged confirmed |
| nitrogen / react-native-nitro-modules | 0.37.1 | **0.37.1 here** | stable Nitro kept for isolated v1 alpha migration PR |

Expo 58 uses `expo-modules-core` on its 58.x release train, currently published at 58.0.29; the downstream QNX peer contract is updated. **Expo Modules API 2.0 beta** is a change in native authoring bindings WITHIN Expo SDK58, NOT `expo-modules-core@2`. It needs a separate native bridge review, not a fake major-version bump.

## Material risk and blocking actions

- This PR intentionally does **not** fabricate a pnpm lockfile. Run `scripts/regenerate-sdk58-lockfile.sh` from Node26/pnpm12 on a networked host, commit the generated `pnpm-lock.yaml`, and run frozen installs.
- `scripts/validate-dependencies.mjs` checks Expo's tested RN/React versions and Metro peer contracts; it must pass before merging. If expo@58.0.22 expects a different RN 0.88 patch, choose the exact Expo-tested patch after inspecting installed `expo/package.json` rather than weakening the check.
- `cmake/dependencies/ReactNativeThirdParty.cmake` pins C++ libraries against the previous RN RC; audit installed RN 0.88.1 podspec versions, regenerate CMake pins and ensure source symbols match.
- Hermes source/bytecode version must come from installed RN 0.88.1 `sdks/.hermesv1version` and `hermes-compiler`; rebase any QNX Hermes patch against that exact source.
- Old `qnx` Metro output and compiled QNX Screen/Qt target must not regress; test the Marathon Metro platform PR separately.
- Re-audit pnpm overrides, patched `braces`, `xmldom`, `uuid`, `js-yaml` and every security exception under the new graph. Don't relabel raw audit warnings clean without patched-source proof.
- Complete the repo's standalone SDK58, host renderer, CMake and QNX target proof queue. This PR alters JS/package metadata only, NOT compiled C++ runtime ABI.
- The `expo-modules-core` peer v58.0.29 is a target based on published SDK58; verify it matches Expo's actual resolved dependency before merge.

## Reproduction / exit criteria

```sh
corepack pnpm install --lockfile-only
corepack pnpm install --frozen-lockfile
corepack pnpm validate:dependencies
corepack pnpm typecheck
corepack pnpm test:js
corepack pnpm audit:dependencies
# rerun production QNX Metro bundle, real Qt/Hermes host, CMake/QNX target gates
```

Only check off a command when its actual exit code and log are attached. Inspect the resulting lockfile rather than copying a guessed version map.

## Why not Nitro v1 in this PR?

Nitro and Nitrogen v1 are pre-release alpha-nightlies and their runtime C++ source was reorganized into a new `nitro-core` package. The QNX Nitro 0.37 CMake source list and `HybridObjectRegistry` APIs will not compile unchanged. Keep this stable RN/Expo upgrade independent and follow with a dedicated v1 migration PR (see companion PR).

## Verified Metro dependency compatibility gate

The networked lockfile-refresh job confirmed that both
`@react-native/metro-config@0.88.1` and `@expo/metro` in
`expo@58.0.22` resolve `metro-config@0.87.1` / require
`metro@0.87.1`. Direct `metro@0.87.2` is newer on npm but
**fails the pinned Expo Metro dependency contract**; this repo must
use `metro@0.87.1` until Expo's release train supports 0.87.2.
The dependency validator therefore retains exact Metro consistency
checks rather than weakening them to permit an unsupported mix.

## xcode v4 / UUID transitive security contract

The installed `expo@58.0.22` graph resolves Apache `xcode@4.0.0`. Its documented source no longer depends on the external `uuid` package: it calls Node's `crypto.randomUUID()` instead. Removed the obsolete `xcode>uuid` override for this release line. The validator now audits whichever interface the **installed** xcode major actually uses: v4 native crypto; v3 transitive `uuid@11.1.1` with CJS v4. This is a source-derived API migration, not a blanket security-audit exception.

## Expo Metro 58 out-of-tree platform source change

The published `@expo/metro-config@58.0.20` no longer hardcodes a literal `'macos'` platform in its resolver list. Its source uses `['ios', 'android', ...OUT_OF_TREE_PLATFORMS]`. The dependency validator now checks the current out-of-tree registration symbol instead of an obsolete hardcoded platform name. Marathon remains a separate explicit Metro platform; Expo's default list is not assumed to contain it.

## Regenerated dependency graph

A branch-scoped GitHub Actions run using Node 26.10.0 and pnpm 12.8.1 has now regenerated and committed the exact `pnpm-lock.yaml` (933 changed lines). The generated graph passed `pnpm install --frozen-lockfile` in the regeneration job. **This is not final compatibility proof:** native React Native/Hermes source checks, TypeScript, host renderer and security audit remain subject to normal CI. The one-shot write-enabled workflow was removed after committing the lockfile; remaining CI is read-only.
