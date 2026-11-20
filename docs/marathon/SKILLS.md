# Exact skills and proof policy - MarathonOS integration

Read actual source SKILL.md before an implementation claim. Below are **required skill selections**, not a statement that they were installed or executed. The commit SHAs below were resolved against GitHub on 2026-11-19 and every path in this file exists at its SHA. Treat skills as development guidance, not a substitute for QNX/Marathon upstream source.

## Mandatory API-first Nitro skills (existing Nitro Forge roster)

The existing Nitro Forge conventions take precedence over generic guesses about
Nitro. For every native/JSI module API, read the exact sources before writing
any method shape or handwritten bridge. Source inspected November 2, 2026:
[Margelo react-native-skills](https://github.com/margelo/react-native-skills)
commit `1e9e17be6f41c838db472d6a4b943fc26355a3be`.

Apply the ordered skill pipeline:

1. `api-design`: public TypeScript shape, typed call sites, errors, disposal,
   cancellation, and platform promises.
2. `build-nitro-modules`: `.nitro.ts` generated specs, Nitrogen outputs,
   native implementation, ownership and codegen testing.
3. `cpp`: RAII and JSI lifetime, thread/Qt ownership, CMake includes,
   signal/callback discipline. Use `kotlin` and `swift` only for real
   Android/Apple implementation slices.
4. `nitro-fetch`: opt-in only for HTTP/WebSocket/TextDecoder work. No
   assumption that Nitro Fetch currently supports QNX or Marathon.
5. `react-native-mmkv`: opt-in only for a persistent-store task; verify the
   latest v4 API and native/OS implementation before claiming support.
6. Callstack `react-native-best-practices`,
   `upgrading-react-native`, repository native test gates,
   `github-actions` and `create-react-native-library`, each
   when its code/CI/packaging scope warrants it.
7. Expo `expo-module` and relevant SDK upgrade/migration skill before
   changing Expo Modules or Expo config. SDK 58 Expo Modules API 2.0 beta
   currently documents Swift/Kotlin bindings; QNX/Marathon JSI compatibility
   must be proved independently.

The upstream v1 Nitrogen source inspected for PR #38 currently restricts
generation to Apple/Android, **not** QNX or Marathon. Do not fabricate
generated QNX code. The hand-written v1 HybridClass is a synchronous
experimental proof only. The long-term objective is a properly generated
QNX/Marathon native platform target with a typed API and real tests.

The Callstack `skills/` directory has no `react-native-testing` or `dogfood` skill. At `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` both exist only as vendored copies under `plugins/vendored/.agents/skills/` (with `agent-device`), which is not the canonical `skills/` tree. Use the repository's real JS/C++/device gates for tests.

Selected source references:

- [Margelo API design](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/api-design)
- [Margelo Nitro modules](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/build-nitro-modules)
- [Margelo C++](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/cpp)
- [Margelo Nitro networking](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/react-native-nitro-fetch) (the folder is `react-native-nitro-fetch`; the skill's frontmatter name is `nitro-fetch`)
- [Expo Modules](https://github.com/expo/skills/blob/a9e5bd3fa00b4e61f632e43f85da73aee1942582/plugins/expo/skills/expo-module/SKILL.md)
- [Callstack skills](https://github.com/callstackincubator/agent-skills/tree/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills)

Do not claim a skill was installed or run just because source SKILL.md was
inspected. Require PR proof rows for installed version, applied decision,
implementation, test output and any incomplete device evidence.

## Tier 0: relevant for every PR

| Skill (exact)               | Canonical repository                                           | Trigger                     | Required outcome                                          |
| --------------------------- | -------------------------------------------------------------- | --------------------------- | --------------------------------------------------------- |
| expo-overview               | https://github.com/expo/skills                                 | Expo/Metro module work      | SDK architecture and correct platform boundaries          |
| expo-upgrade                | https://github.com/expo/skills                                 | package upgrades            | Expo 58/RN 0.88 peer and breaking-change checks           |
| react-native-best-practices | https://github.com/callstackincubator/agent-skills             | every native implementation | event loop, startup, memory, Fabric performance review    |
| upgrading-react-native      | https://github.com/callstackincubator/agent-skills             | RN 0.88 rc -> stable        | upstream code/template differences + native source audit  |
| writing-user-docs           | https://github.com/callstackincubator/agent-skills             | test and developer guides   | reproducible instructions and honest hardware limitations |
| github-actions              | https://github.com/callstackincubator/agent-skills             | CI change                   | deterministic artifacts and security/scoped permissions   |
| code-review                 | trusted owner's installed skill bundle only; do not invent URL | each PR if installed        | independent blocker and regression review                 |

## Tier 1: when affected

| Skill (exact)               | Canonical repository                                                                                               | Applicable scope                                         |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------- |
| expo-router                 | https://github.com/expo/skills                                                                                     | Expo Router example, navigation, deeplinks               |
| expo-native-ui              | https://github.com/expo/skills                                                                                     | native RN/Qt views and behavior                          |
| expo-design-system          | https://github.com/expo/skills                                                                                     | native UI conventions, accessibility                     |
| expo-data-fetching          | https://github.com/expo/skills                                                                                     | network module, cancellation, offline/retry, permissions |
| expo-animation              | https://github.com/expo/skills                                                                                     | reduced motion, native Animated timing                   |
| react-navigation            | https://github.com/callstackincubator/agent-skills                                                                 | Back, stack routing, navigation                          |
| agent-device                | https://github.com/callstackincubator/agent-skills (vendored under `plugins/vendored/.agents/skills/agent-device`) | iOS/Android automation only where supported              |
| create-react-native-library | https://github.com/callstackincubator/agent-skills                                                                 | publishing stand-alone Nitro/Expo platform libraries     |

## Platform-specific research (not pretending these are installed agent skills)

Authoritative materials must also be read before the associated slice: Qt 6.11.1+ Qt Quick QML and QtWayland docs; React Native 0.88 renderer and Hermes bundled tags; `patrickjquinn/Marathon-Shell` app runner, manifest schema, Wayland, security and lifecycle source; `mrousavy/nitro` Nitro 1.0 prerelease docs and exact dist-tag; `expo/expo` Modules API 2.0 beta release notes/code; npm dist-tags and peer ranges. Call these **source audits**, never a skill.

Install selected skills using repository-scoped commands, for example:

```bash
npx skills@latest add expo/skills --skill expo-overview
npx skills@latest add expo/skills --skill expo-upgrade
npx skills@latest add expo/skills --skill expo-native-ui
npx skills@latest add callstackincubator/agent-skills --skill upgrading-react-native
npx skills@latest add callstackincubator/agent-skills --skill react-native-best-practices
npx skills@latest add callstackincubator/agent-skills --skill writing-user-docs
npx skills@latest add callstackincubator/agent-skills --skill github-actions
```

Check exact skill names and repository state before running, and use the team's actual skill installer for other tools. Pin the resolved source commit; `@latest` installs are not immutable evidence.

## Proof table (filled 2026-11-19)

Upstream SHAs: Margelo `1e9e17be6f41c838db472d6a4b943fc26355a3be` (2026-08-21, still the head of `main`), Callstack `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` (2026-10-08, still the head of `main`), Expo `a9e5bd3fa00b4e61f632e43f85da73aee1942582` (2026-11-01; `main` has moved to `88d9bd79f3fa`). "Blob" is the git blob SHA of the SKILL.md at that commit. "Read evidence" says how much of the file was read through `gh api`. No skill was installed with `npx skills` for these rows, and none ran as a tool. Other documents ([docs/nitro/SKILLS_AND_ROSTER.md](../nitro/SKILLS_AND_ROSTER.md), [skills.lock.json](../expo-delivery/skills.lock.json)) record decisions attributed to these skills; the table cites only decisions that code or CI in this repository reflects.

| Exact skill                 | Upstream path, SHA, blob                                                                                                                       | Scope                                                                      | Read evidence                           | Decision changed                                                                                                                                                                                                                                                                                                                                                                                                                                                      | Test or artifact                                                                                                                                                   | Rejected alternative and reason                                                                                                                                    |
| --------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------- | --------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| api-design                  | `margelo/react-native-skills` @ `1e9e17be`, `skills/api-design/SKILL.md`, blob `3768944d5e87`                                                  | Marathon Nitro adapter surface                                             | Frontmatter and section headings read   | The Marathon adapter exposes a narrow `MarathonPlatformInfo` class that mirrors the typed `QnxPlatformInfo` contract in [API_CONTRACTS](../nitro/API_CONTRACTS.md)                                                                                                                                                                                                                                                                                                    | `tests/js/nitro-dual-host-contract.test.mjs`; `nitro-v1-host` compile ([run 44492351495](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351495)) | None recorded. I cannot show the skill drove the Marathon shape; the QNX shape came first                                                                          |
| build-nitro-modules         | same SHA, `skills/build-nitro-modules/SKILL.md`, blob `82dfbb732d16`                                                                           | Nitro v1 spec, Nitrogen, native class                                      | Frontmatter and section headings read   | No fabricated Nitrogen output for QNX or Marathon. `scripts/nitro-platform-codegen.mjs` emits the descriptor binding instead, and [API_CONTRACTS](../nitro/API_CONTRACTS.md) records that Nitrogen only generates for `apple` and `android`                                                                                                                                                                                                                           | `tests/js/nitro-platform-codegen.test.mjs`; `nitro-v1-host` compile. No Marathon runtime run                                                                       | Writing Nitrogen-looking output by hand: this file's policy above forbids fabricated generated QNX code. A Nitrogen QNX or Marathon target does not exist upstream |
| cpp                         | same SHA, `skills/cpp/SKILL.md`, blob `b0770da7ba90`                                                                                           | Linux adapter C++: ownership, callbacks                                    | Read in full                            | Not verified for Marathon. The skill's rules (RAII, no callback under a mutex, no sleep-based race fixes) are review criteria. I did not review `MarathonNitroRuntime.cpp` against them                                                                                                                                                                                                                                                                               | `nitro-v1-host` proves the adapter compiles. Nothing proves lifetime or threading behavior on Marathon                                                             | None recorded                                                                                                                                                      |
| react-native-best-practices | `callstackincubator/agent-skills` @ `59e9d174`, `skills/react-native-best-practices/SKILL.md`, blob `86ec5ab5a4cadf454db8013293679891098efce1` | Qt/Fabric performance                                                      | Frontmatter and section headings read   | None traceable to Marathon code or CI. [skills.lock.json](../expo-delivery/skills.lock.json) records "measure the real interaction and target"                                                                                                                                                                                                                                                                                                                        | No Marathon performance measurement exists, so there is no artifact                                                                                                | None recorded. [ARCHITECTURE](ARCHITECTURE.md) treats performance targets as hypotheses until measured                                                             |
| github-actions              | same SHA, `skills/github-actions/SKILL.md`, blob `8f420a3328f8cfd0e85c6ee89e5b5a0c45beb39d`                                                    | Marathon CI                                                                | Read in full                            | Partial. The skill covers iOS and Android simulator builds, which do not apply. What carries over: `permissions: contents: read` and SHA-named evidence artifacts (`marathon-native-qt-${{ github.sha }}`, `marathon-metro-${{ github.sha }}`, `nitro-v1-host-${{ github.sha }}`) in `.github/workflows/marathon-qt-host.yml`, `marathon-metro.yml`, `nitro-v1-host.yml`. Only `nitro-v1-host.yml` pins actions by commit SHA; the other two use `@v4` and `@v5` tags | The three successful runs listed in [NATIVE_HOST](NATIVE_HOST.md)                                                                                                  | The skill's composite actions are not used: they build iOS and Android apps                                                                                        |
| create-react-native-library | same SHA, `skills/create-react-native-library/SKILL.md`, blob `3613cd0735ecb263cd100c2057d4cb4844e046f0`                                       | `@rn-qnx/metro-config-marathon`, `@rn-qnx/react-native-marathon` packaging | Frontmatter and the first 60 lines read | No evidence of a decision. The skill scaffolds TurboModule or view libraries with `npx create-react-native-library`. These two packages are resolver and JS-override packages, not scaffolded that way                                                                                                                                                                                                                                                                | Packed contents are recorded in [pack-contents-2026-11-12-b51b49c](../evidence/pack-contents-2026-11-12-b51b49c.md); that receipt is not a use of the skill        | Copying a mobile TurboModule scaffold: [skills.lock.json](../expo-delivery/skills.lock.json) says not to                                                           |
| upgrading-react-native      | same SHA, `skills/upgrading-react-native/SKILL.md`, blob `ead57b7676d0448f4d87c1988a8717a5349930f1`                                            | RN 0.88 rc to stable                                                       | Frontmatter and the first 40 lines read | None supported. The repository moved to `react-native` 0.88.1 and `expo` 58.0.24, and CI runs a "Frozen SDK58 workspace" step. I found no record that the skill's rn-diff-purge flow was used. That flow targets iOS and Android templates, and no QNX or Marathon template exists                                                                                                                                                                                    | Lockfile pins; `marathon-metro` run above                                                                                                                          | Template-diff upgrade: not applicable to a Qt host                                                                                                                 |
| expo-module                 | `expo/skills` @ `a9e5bd3f`, `plugins/expo/skills/expo-module/SKILL.md`, blob `ade758421aee5fd5cf7d948cf6baae156d3a3c7b`                        | Expo Modules API on Marathon                                               | Frontmatter and section headings read   | None for Marathon. No Expo module is registered in the Marathon host. [skills.lock.json](../expo-delivery/skills.lock.json) records "discovery is not runtime registration"                                                                                                                                                                                                                                                                                           | None                                                                                                                                                               | None recorded. No Expo Modules API 2 support is claimed for Marathon                                                                                               |
| expo-upgrade                | same SHA, `plugins/expo/skills/expo-upgrade/SKILL.md`, blob `db32f2c5552085601e17c4b8e46f907dea24b057`                                         | Expo SDK 58 package graph                                                  | Read in full                            | Consistent, not attributable. The skill warns that SDK 55 to 57 builds before `expo@57.0.9` have a Hermes V1 memory regression with `react-native-worklets` and `react-native-reanimated`. The lockfile has `expo` 58.0.24, past that line, and PR #149 ships Worklets and Reanimated. No record shows the skill drove that pin                                                                                                                                       | `pnpm validate:dependencies` in `marathon-metro`                                                                                                                   | Worklets Bundle Mode as a workaround: the skill calls it unsupported                                                                                               |

## Corrections found in this pass

- `Argent/agent-device` does not exist on GitHub (404). The `agent-device` skill is vendored in Callstack's repository.
- `react-native-testing` and `dogfood` exist at the Callstack SHA only under `plugins/vendored/.agents/skills/`.
- The Margelo `react-native-nitro-fetch` folder holds a skill named `nitro-fetch`.
- All other paths and names in this file resolve at the pinned SHAs.

QNX/Marathon tests are still required after skills. A skill row stays unverified where the table says so.
