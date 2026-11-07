# Phase 1: an external Expo application that runs on QNX

Status: proposed implementation plan; this PR changes documentation only.
Source baseline: `cfc28165525e0d7c54192daf58b9b4318a26e83a` (2026-11-07).
Implementation predecessor: reviewed [Phase0](https://github.com/mikevocalz/react-native-qnx/pull/81) integration. Planning review does not require merging another plan.

## Outcome and existing gap

Create or adopt one Expo application in a normal consumer directory, using public React Native/Expo imports, its own app identity and shared source. Build, install and launch the full native application on the named QNX target without modifying platform source. Keep the product named React Native QNX; QNX is default and MarathonOS is an additional target.

The [source CLI](../../packages/react-native-qnx-cli/src/cli.mjs), [template](../../template/qnx/index.js), [README](../../README.md) and [integrated candidate #80](https://github.com/mikevocalz/react-native-qnx/pull/80) supply groundwork. They do not yet remove the checkout requirement or hard-coded playground identity. Existing full-runtime and recovery behavior stays intact. Reuse old runtime/package/conformance workstreams 01/10/12, not a new renderer.

## Consumer contract to settle before coding

The target-aware CLI accepts a consumer project root independently of its own installation location. QNX is the default; an explicit Marathon target selects the other adapter. The exact command grammar is a design deliverable, not a claim that stock Expo supports custom `run` commands today. Reject unsupported target/options before network or filesystem side effects.

Configuration carries app ID, display name, entry module, surface registration, assets, target architecture and native build settings. Preserve existing mobile/web settings when adopting an app. Public `react-native` imports must resolve supported components and platform modules; internal `Libraries/...` imports may remain internal implementation details, never consumer requirements. Unsupported required exports fail with actionable diagnostics rather than a fake component.

Preparation owns generated paths through a versioned manifest. Creation never overwrites an existing app. Adoption produces a reviewable change set and preserves user code. Define repeat preparation, moved project roots, custom entry points, paths with spaces, two apps built concurrently and interrupted generation. Each app gets independent bundle/native/output directories; no cross-app identity or cache contamination.

## Roster and path ownership

All human assignments are unassigned; implementation authors and independent reviewers must be different people/roles at approval.

- CLI/DX engineer: `packages/react-native-qnx-cli/` and proposed consumer-project commands; reviewer: package/tooling engineer. Deliverable: external-root resolution, bounded subprocesses, safe generation and failure diagnostics.
- Metro/public-API engineer: `packages/metro-config-qnx/`, `packages/metro-config-marathon/`, `packages/react-native-qnx/`, `packages/react-native-marathon/`; reviewer: RN runtime engineer. Deliverable: public imports, platform identity and preserved existing-target resolution.
- QNX native runner engineer: `vnext/probes/playground/main.cpp`, shared host interfaces, `cmake/`, `native/platform/qnx/`; reviewer: Qt/JSI lifetime engineer. Deliverable: per-app identity/entry configuration without replacing the existing runner or legacy probes.
- Template/config engineer: `template/qnx/`, `packages/expo-qnx/` after prerequisite integration, proposed external template fixtures; reviewer: Expo/tooling engineer. Deliverable: consumer package contents, config preservation and correct assets.
- Target QA engineer: proposed `tests/integration/external-app/`, existing JS/native tests and device runbooks; reviewer: independent QNX device owner. Deliverable: literal fresh-directory create/build/install/run receipt.
- Distribution engineer: package exports, Qt/plugin closure and documentation; reviewer: dependency/licensing reviewer. Deliverable: local test tarballs first, publication only after Phase6/7 authorization and content checks.

Paths described as proposed are ownership reservations, not existing features. Shared native files require the Phase0 integrator's sequencing.

## Skills and applied decisions

Read [repository native workflow](../../.agents/skills/qnx-native-api/SKILL.md), [validation skill](../../.agents/skills/qnx-platform-validation/SKILL.md), [skill policy](../platform-modernization/SKILLS.md) and [actual Nitro v1 policy](../platform-modernization/NITRO_V1_API_POLICY.md).

- Margelo `api-design`, `build-nitro-modules`, `cpp`, ordered at `1e9e17be6f41c838db472d6a4b943fc26355a3be`, paths `skills/<name>/SKILL.md`: explicit app/runner contracts, generated ABI and Qt/JS ownership for native changes. Do not introduce a Nitro wrapper merely for CLI work; apply the full chain when the slice exposes a native API.
- Callstack `create-react-native-library` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`: consumer package boundaries and packed-file verification. Its mobile/TurboModule scaffold examples do not authorize another bridge or generated QNX support.
- Callstack `github-actions` and `react-native-best-practices` at the same revision: exact candidate artifacts and real startup measurements; iOS/Android automation is not target proof.
- Expo `expo-module` at `a9e5bd3fa00b4e61f632e43f85da73aee1942582`, path `plugins/expo/skills/expo-module/SKILL.md`: preserve configuration and distinguish explicit native support from discovery.
- Callstack `writing-user-docs` at the pinned revision is required when writing the actual consumer guide: prerequisites, one verified path and failure recovery.

Planning used source guidance; native execution/skill installation is not claimed. Implementers record exact source-read and installation receipts plus the decision changed. Conditional upgrade/migration skills must be discovered and pinned before a version change. Retain the matched Nitro v1 baseline; do not copy older APIs from generic examples.

## Small implementation PRs

1. Extract project discovery/configuration from monorepo assumptions; test create/adopt/no-overwrite with two consumer roots. Do not relocate the renderer or rename packages.
2. Package public entry/resolver/template files and test from packed artifacts outside the checkout. Bundle the same source for QNX and Marathon while preserving supported mobile/web targets.
3. Add an explicit app descriptor to the existing host: app ID, registered surface, bundle/bytecode, assets and output identity. Verify Hermes compiler/runtime matching and Qt initialization/shutdown. Keep historical playground defaults for existing probes.
4. Generate a native QNX project using the selected SDK/toolchain and qualified Qt closure. Build the full React Native target, not `rnqnx_playground` alone. Implement bounded install/launch with hashes and rollback; no implicit flash or boot edit.
5. Walk the consumer guide on a new provisioned development host and the named QNX board. Publish the scope as a minimal application alpha, not broad Expo-module or Router parity.

## Tests and completion gate

Host tests cover public imports, bad paths, invalid IDs, option rejection, moved roots, generated-file drift, interruption, duplicate creation, missing SDK/Qt, wrong architecture and absent native runner. Missing prerequisites must fail before deployment. Compare QNX resolver outputs before/after Marathon changes. Use frozen install, `pnpm typecheck`, `pnpm test:js`, `pnpm validate:dependencies`, `pnpm validate:architecture` and `pnpm codegen:check`; add registered external-project tests, not source-string assertions alone.

Native changes compile/run feature OFF/ON with relevant sanitizer coverage. Capture exact source, lockfile, runner, bundle, toolchain and flags. An offscreen screenshot is supporting evidence only.

- [ ] External app contains no platform-checkout paths or consumer RN internal imports.
- [ ] Two app identities and build outputs cannot collide.
- [ ] One shared source bundles for both target identities; QNX has no Marathon dependency.
- [ ] Full QNX application builds, installs, launches, renders and exits on the named device.
- [ ] Missing/unsupported capability and deployment failures are actionable and nonzero.
- [ ] Consumer steps are reproduced by a reviewer without platform source edits.

## Security, performance, limitations and rollback

Use verified target identity, bounded argument vectors, explicit generated-file ownership and no shell interpolation of app data. Do not redistribute QNX SDK/evaluation payloads or vendor premium products. Follow ADR0013's separately qualified eligible Qt source build. Record native startup/memory baselines without making FPS or clean-boot claims.

This phase does not promise Expo Go, EAS custom-target support, every npm module, Router, managed Marathon packaging or debugger attachment. Those are later gates. Roll back each native slice behind its feature switch, preserve existing runner defaults and user app source, and restore matching generated/config artifacts. Reverting this planning file alone changes no runtime.
