# ADR-0008: Current-compatible version pinning and source contracts

## Status

Accepted

## Context

React Native, React, Expo, Metro, Hermes, Nitro Modules, Nitrogen, TypeScript,
Node, and pnpm move on independent release cadences. An out-of-tree platform
cannot safely mix arbitrary "latest" packages when Expo or React Native declares
a specific compatibility line, but it also should not stay frozen on an old
ecosystem after that line moves.

The project therefore distinguishes:

1. exact direct workspace pins;
2. exact transitive versions resolved by the lockfile;
3. source snapshots bundled inside React Native; and
4. QNX-specific patches whose applicability must be re-proven after upstream
   source changes.

None of those identities alone demonstrates QNX support. Native seams must be
tied to installed source and then separately compiled and exercised with the
licensed QNX SDK/target.

## Decision

Track the newest requested Expo SDK line, use the React/React Native versions
that Expo itself tests for that SDK, and keep the remaining direct npm tooling
on current published versions when they install coherently. Pin every direct
dependency exactly.

As of 2026-10-01 the direct development line is:

- Expo 58.0.0 / SDK 58
- React Native 0.88.0-rc.3
- React 19.3.0
- Metro 0.87.1
- `@react-native/metro-config` 0.88.0-rc.3
- `babel-preset-expo` 58.0.4
- React Native Nitro Modules 0.37.1
- Nitrogen 0.37.1
- TypeScript 7.0.2
- `@types/react` 19.3.0
- `@types/node` 26.6.3
- Prettier 3.9.9
- Node.js 26+
- pnpm 12.8.1

Expo 58.0.0 is intentionally selected even though npm still labels SDK 57 as
`latest` and SDK 58 as `next` on 2026-10-01. SDK 58 is the product target,
and its published Expo manifest tests React 19.3.0 with React Native
0.88.0-rc.3.

The regenerated lockfile currently resolves:

- `expo-modules-core` 58.0.9
- `@expo/metro` 58.0.0-rc.0
- `@expo/metro-config` 58.0.6
- `hermes-compiler` 260318099.0.4
- bundled Hermes source tag `hermes-v260318099.0.2`

Security overrides remain scoped to their consumer and require a source
call-site check. The current `xcode>uuid` override resolves uuid 11.1.1; the
validator confirms that installed `xcode/lib/pbxProject.js` still calls
`uuid.v4()` and that the overridden CommonJS package exports that function.

React Native's installed `sdks/.hermesv1version` is the source of truth for
the bundled Hermes identity. The previous QNX Hermes work was performed against
the RN 0.86 Hermes line `250829098.0.16`. Its patch is historical evidence
only until rebased and revalidated against the SDK-58/RN-0.88 Hermes source line
`260318099.0.2` (with `hermes-compiler` 260318099.0.4).

Yoga is recorded only as the source snapshot bundled with the selected React
Native package. The project does not invent a standalone Yoga version.

## Executable source contract

`scripts/validate-dependencies.mjs` validates:

- every direct workspace dependency is an exact installed pin;
- Expo is SDK 58 and its tested React/React Native versions match the workspace;
- the selected React Native peer range accepts React 19.3.0;
- Nitro Modules and Nitrogen remain on one compatible release line;
- Expo's resolved core/Metro packages remain on SDK 58;
- the RN Metro config resolves the selected Metro 0.87.x line;
- React Native's Hermes source tag matches its installed compiler identity;
- selected Fabric, Yoga, Expo Modules, Nitro, and Metro source symbols still
  exist.

Those checks detect upstream source-shape drift. They do not claim that Hermes,
Nitro Modules, Expo Modules Core, Metro, or Fabric supports QNX without the
separate QNX build/runtime gates.

## Update procedure

1. Check the requested Expo SDK and its published React/React Native baseline.
2. Query current registry releases for the remaining direct packages.
3. Select exact pins and regenerate `pnpm-lock.yaml` from the npm registry.
4. Run `pnpm validate:dependencies` and repair source contracts based on the
   actually installed packages.
5. Run TypeScript, JavaScript, package, and host-native validation.
6. Update this ADR and the compatibility matrix with the exact resolved graph.
7. Rebase any QNX-specific upstream patches when their source identity changes.
8. Run QNX cross-compilation and physical-target validation before declaring
   the new line QNX-compatible.

## Consequences

- Direct installs are deterministic.
- Expo's SDK contract wins over blindly following an incompatible npm dist-tag.
- Standalone developer tooling can remain current without pretending all
  package families have the same release cadence.
- A React Native/Hermes upgrade invalidates unproven source patches until they
  are rebased and retested.
- Host evidence and QNX hardware evidence remain separate.

## Alternatives considered

- **Floating semver ranges:** rejected because native seams can drift between
  installs.
- **npm `latest` for every package without compatibility checks:** rejected
  because Expo SDK 58 currently requires a React Native 0.88 release-candidate
  line while some React Native support packages still expose 0.87.x as their
  stable dist-tag.
- **Keep SDK 57 because it is still npm `latest`:** rejected because SDK 58 is
  the selected product baseline.
- **Reuse the RN 0.86 Hermes QNX patch unchanged:** rejected because the bundled
  Hermes source identity changed.

## Related files

- `package.json`
- `pnpm-workspace.yaml`
- `pnpm-lock.yaml`
- `scripts/validate-dependencies.mjs`
- `docs/getting-started/compatibility-matrix.md`
- `docs/handoff/HERMES_QNX.md`
- `docs/handoff/QNX_VALIDATION_QUEUE.md`
