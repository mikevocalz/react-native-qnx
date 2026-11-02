# Exact skills and proof policy — MarathonOS integration

Read actual source SKILL.md before an implementation claim. Below are **required skill selections**, not a statement that they were installed or executed. Freeze source commit SHA at implementation time. Treat skills as development guidance, not a substitute for QNX/Marathon upstream source.

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
