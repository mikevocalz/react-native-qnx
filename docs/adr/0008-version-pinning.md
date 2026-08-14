# ADR-0008: Compatible-current version pinning and source contracts

## Status

Accepted

## Context

React Native, React, Expo, Metro, Hermes, Nitro Modules, Nitrogen, and TypeScript move on independent release cadences. Selecting the absolute newest release of each package can produce a graph that the package maintainers do not declare or test together. Floating ranges also make source-level native seams drift between installs.

An out-of-tree QNX platform must distinguish three kinds of identity:

1. exact direct workspace dependencies;
2. exact transitive versions resolved by the lockfile; and
3. source snapshots bundled inside React Native.

None of those identities alone demonstrates QNX support. Native seams must be tied to an installed source file and symbol, then separately compiled and exercised with the licensed QNX SDK/target.

## Decision

Use the newest audited **mutually compatible release line**, then pin every direct dependency exactly. Do not advance React Native, React, Expo, or Metro independently when their installed manifests identify a different tested/peer baseline.

The current direct line is:

- React Native 0.86.2
- React 19.2.3
- Expo 57.0.12
- Metro 0.84.4
- React Native Nitro Modules 0.36.5
- Nitrogen 0.36.5
- TypeScript 6.0.3
- pnpm 11.21.0

The current locked Expo graph is recorded separately:

- `expo-modules-core` 57.0.10
- `@expo/metro` 56.0.0, resolving upstream Metro 0.84.4
- `@expo/metro-config` 57.0.8

Security overrides are scoped to the installed consumer and require a source
call-site check. The current `xcode>uuid` override resolves uuid 11.1.1; the
validator confirms installed `xcode/lib/pbxProject.js` calls `uuid.v4()` and that
the overridden CommonJS package still exports that function. Audit findings with
no published compatible fix are recorded as blockers and are not silently
ignored.

React Native's installed `sdks/.hermesv1version` records `hermes-v250829098.0.16` (version component `v250829098.0.16`). Yoga is recorded only as the source snapshot bundled with React Native 0.86.2; the project must not invent a standalone Yoga version.

`scripts/validate-dependencies.mjs` is the executable version/source contract. In addition to versions and compatibility declarations, it checks these installed native source seams:

- React Native `ReactCommon/react/renderer/scheduler/Scheduler.h` → `class Scheduler final`
- React Native `ReactCommon/react/renderer/mounting/MountingCoordinator.h` → `class MountingCoordinator final`
- React Native `ReactCommon/yoga/yoga/YGNode.h` → `YGNodeCalculateLayout`
- Nitro Modules `cpp/registry/HybridObjectRegistry.hpp` → `registerHybridObjectConstructor`
- Nitro Modules `cpp/entrypoint/InstallNitro.hpp` → `void install(`
- Expo Modules Core `android/src/main/cpp/ExpoModulesHostObject.h` → `class ExpoModulesHostObject`

These checks prove that the audited source contracts are present in the installed packages. They make no claim that Hermes, Nitro Modules, Expo Modules Core, Metro, or Fabric already supports QNX. Every future QNX adapter must be validated independently and tracked in `docs/handoff/QNX_VALIDATION_QUEUE.md`.

## Update procedure

1. Query current releases for all coupled packages.
2. Choose the newest graph whose installed peer/development/dependency declarations agree.
3. Install with the candidate exact direct pins and regenerate `pnpm-lock.yaml`.
4. Inspect installed native source and update every file/symbol contract that changed.
5. Update the compatibility matrix and this ADR.
6. Run `pnpm validate:macos` or the equivalent Linux host validation.
7. Perform the queued compile/runtime checks on the licensed Ubuntu/QNX host and target before declaring QNX compatibility.

## Consequences

- Installs are reproducible at the direct and lockfile levels.
- The compatibility line can lag an independently published latest package when the newest releases are not mutually compatible.
- Bundled source identities are explicit without assigning unsupported semantic versions.
- Upstream source-shape drift fails validation before a native adapter silently compiles against the wrong contract.
- Host source-contract checks and QNX compatibility claims remain deliberately separate.

## Alternatives considered

- **Semver ranges:** rejected because installs can drift without review and break native source contracts.
- **Absolute latest of every package:** rejected because individually current releases are not necessarily a declared/tested combination.
- **Version pins without source-symbol checks:** rejected because a package version alone is insufficient evidence for a native seam.
- **Treating bundled Yoga or Hermes as an inferred standalone release:** rejected because that would fabricate provenance not established by installed React Native source.

## Related files

- `package.json`
- `pnpm-workspace.yaml`
- `pnpm-lock.yaml`
- `scripts/validate-dependencies.mjs`
- `docs/getting-started/compatibility-matrix.md`
- `docs/handoff/QNX_VALIDATION_QUEUE.md`
