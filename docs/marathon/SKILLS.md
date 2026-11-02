# Exact skills and proof policy — MarathonOS integration

Read actual source SKILL.md before an implementation claim. Below are **required skill selections**, not a statement that they were installed or executed. Freeze source commit SHA at implementation time. Treat skills as development guidance, not a substitute for QNX/Marathon upstream source.

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
   `upgrading-react-native`, `react-native-testing`,
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

Selected source references:

- [Margelo API design](https://github.com/margelo/react-native-skills/tree/main/skills/api-design)
- [Margelo Nitro modules](https://github.com/margelo/react-native-skills/tree/main/skills/build-nitro-modules)
- [Margelo C++](https://github.com/margelo/react-native-skills/tree/main/skills/cpp)
- [Margelo Nitro networking](https://github.com/margelo/react-native-skills/tree/main/skills/react-native-nitro-fetch)
- [Expo Modules](https://github.com/expo/skills/blob/main/plugins/expo/skills/expo-module/SKILL.md)
- [Callstack skills](https://github.com/callstackincubator/agent-skills/tree/main/skills)

Do not claim a skill was installed or run just because source SKILL.md was
inspected. Require PR proof rows for installed version, applied decision,
implementation, test output and any incomplete device evidence.

## Tier 0: relevant for every PR

| Skill (exact) | Canonical repository | Trigger | Required outcome |
| --- | --- | --- | --- |
| expo-overview | https://github.com/expo/skills | Expo/Metro module work | SDK architecture and correct platform boundaries |
| expo-upgrade | https://github.com/expo/skills | package upgrades | Expo 58/RN 0.88 peer and breaking-change checks |
| react-native-best-practices | https://github.com/callstackincubator/agent-skills | every native implementation | event loop, startup, memory, Fabric performance review |
| upgrading-react-native | https://github.com/callstackincubator/agent-skills | RN 0.88 rc -> stable | upstream code/template differences + native source audit |
| react-native-testing | https://github.com/callstackincubator/agent-skills | tests and CI | host unit, native conformance and user-centered assertions |
| github-actions | https://github.com/callstackincubator/agent-skills | CI change | deterministic artifacts and security/scoped permissions |
| code-review | trusted owner's installed skill bundle only; do not invent URL | each PR if installed | independent blocker and regression review |

## Tier 1: when affected

| Skill (exact) | Canonical repository | Applicable scope |
| --- | --- | --- |
| expo-router | https://github.com/expo/skills | Expo Router example, navigation, deeplinks |
| expo-native-ui | https://github.com/expo/skills | native RN/Qt views and behavior |
| expo-design-system | https://github.com/expo/skills | native UI conventions, accessibility |
| expo-data-fetching | https://github.com/expo/skills | network module, cancellation, offline/retry, permissions |
| expo-animation | https://github.com/expo/skills | reduced motion, native Animated timing |
| react-navigation | https://github.com/callstackincubator/agent-skills | Back, stack routing, navigation |
| agent-device | https://github.com/callstackincubator/agent-skills | physical device automation (only if supported) |
| dogfood | https://github.com/callstackincubator/agent-skills | exploratory persona QA on actual test builds |
| create-react-native-library | https://github.com/callstackincubator/agent-skills | publishing stand-alone Nitro/Expo platform libraries |

## Platform-specific research (not pretending these are installed agent skills)

Authoritative materials must also be read before the associated slice: Qt 6.11.1+ Qt Quick QML and QtWayland docs; React Native 0.88 renderer and Hermes bundled tags; `patrickjquinn/Marathon-Shell` app runner, manifest schema, Wayland, security and lifecycle source; `mrousavy/nitro` Nitro 1.0 prerelease docs and exact dist-tag; `expo/expo` Modules API 2.0 beta release notes/code; npm dist-tags and peer ranges. Call these **source audits**, never a skill.

Install selected skills using repository-scoped commands, for example:

```bash
npx skills@latest add expo/skills --skill expo-overview
npx skills@latest add expo/skills --skill expo-upgrade
npx skills@latest add expo/skills --skill expo-native-ui
npx skills@latest add callstackincubator/agent-skills --skill upgrading-react-native
npx skills@latest add callstackincubator/agent-skills --skill react-native-best-practices
npx skills@latest add callstackincubator/agent-skills --skill react-native-testing
npx skills@latest add callstackincubator/agent-skills --skill github-actions
```

Check exact skill names and repository state before running, and use the team's actual skill installer for other tools. Pin the resolved source commit; `@latest` installs are not immutable evidence.

## Proof table template (mandatory in PRs)

| Exact skill | Upstream SHA/version | Scope | Read evidence | Decision changed | Test or artifact | Rejected alternative + reason |
| --- | --- | --- | --- | --- | --- | --- |
| expo-upgrade | TODO | package graph | TODO | TODO | TODO | TODO |
| react-native-best-practices | TODO | Qt/Fabric | TODO | TODO | TODO | TODO |
| react-native-testing | TODO | CI | TODO | TODO | TODO | TODO |

Rows with TODO remain **unverified**; do not check a skill as executed until the proof row is complete. QNX/Marathon tests are still required after skills.
