# Nitro API engineering skills and reference roster

This roster reuses the existing Nitro Forge conventions. It does **not** assert that the listed external agents or people are participating. Skills must be installed/read and their _effect on the patch_ recorded with a version/commit; a skill name alone is not proof.

## Directly inspected skill-source revisions (2026-11-02)

Margelo's maintained `react-native-skills` source:

- Repo https://github.com/margelo/react-native-skills
- HEAD inspected: `1e9e17be6f41c838db472d6a4b943fc26355a3be`
- `api-design`, source `skills/api-design/SKILL.md` — write the public TypeScript contract and 2–3 valid call sites, verify errors and cleanup before native implementation.
- `build-nitro-modules`, source `skills/build-nitro-modules/SKILL.md` — generated `.nitro.ts` + Nitrogen/native binaries, lifetime and ownership. **v1 alpha QNX generator is missing**, so flag hand-written class as proof-only.
- `cpp`, source `skills/cpp/SKILL.md` — RAII, strong native ownership, CMake library hygiene and thread-affine event dispatch.
- `nitro-fetch`, source `skills/react-native-nitro-fetch/SKILL.md` — fetch, WebSocket and TextDecoder, native prewarming; **source auditing only**, not a QNX runtime compatibility claim.

## Exact skill selection

Use these with the real `SKILL.md` files and SHA, not a guessed skill title.

### Tier A — mandatory for Nitro/native API PRs

- `margelo/react-native-skills:api-design`
- `margelo/react-native-skills:build-nitro-modules`
- `margelo/react-native-skills:cpp`
- `callstackincubator/agent-skills:react-native-best-practices`
- `callstackincubator/agent-skills:react-native-testing`
- `callstackincubator/agent-skills:upgrading-react-native` when moving RN/JSI/ABI versions
- `callstackincubator/agent-skills:github-actions` when editing CI
- `expo/skills:expo-module` for Expo-module bridges; `upgrading-expo` or verified current named skill when bumping Expo SDK

### Tier B — conditional on actual code

- `margelo/react-native-skills:kotlin` or `swift`: use only for real Android/Apple implementations.
- `margelo/react-native-skills:nitro-fetch`: HTTP, WebSocket, native decoding, cold-start prewarm. Must validate current API source.
- `margelo/react-native-skills:react-native-mmkv`: encrypted or multi-process native storage, v4 migration.
- `callstackincubator/agent-skills:create-react-native-library`: independent standalone native module packaging and consumer example.
- `callstackincubator/agent-skills:agent-device`, `dogfood`: physical-device QA. Only if the tooling actually supports that OS.
- `expo/skills:expo-dev-client` or current verified Expo development-client skill: a supported native build, never assume Expo Go covers custom QNX/Marathon modules.
- `petergyang/no-ai-slop`: strict evidence and removal of invented success/stubs.
- `WorldFlowAI/everything-claude-code`: planning/testing/review. Inspect PRs and actual source state before claiming content merged.

### Install from audited sources (do not equate these commands with execution evidence)

```bash
npx skills@latest add margelo/react-native-skills --skill api-design
npx skills@latest add margelo/react-native-skills --skill build-nitro-modules
npx skills@latest add margelo/react-native-skills --skill cpp
npx skills@latest add margelo/react-native-skills --skill nitro-fetch
npx skills@latest add callstackincubator/agent-skills --skill react-native-best-practices
npx skills@latest add callstackincubator/agent-skills --skill upgrading-react-native
npx skills@latest add callstackincubator/agent-skills --skill react-native-testing
npx skills@latest add callstackincubator/agent-skills --skill github-actions
```

Pin the source SHA in the actual working environment and record how each skill changed the implementation. Never execute downloaded scripts without review.

## Contributor roster / accountability

- **API architect (DRI):** public TypeScript contract, explicit errors/capabilities, 2–3 call sites; reviews `index.d.ts` and `docs/nitro/API_CONTRACTS.md`.
- **Nitrogen/codegen engineer:** compare v0.37/v1 alpha generator, propose QNX/Marathon target extensions without faking generated output; own source/determinism tests.
- **C++ Nitro/JSI engineer:** shared HybridClass registry, correct v1 includes/sources, RAII, no cross-runtime stale pointers; own `qnx-nitro-runtime/cpp`.
- **Qt/event-loop engineer:** typed Promise/dispatcher callbacks, cancellation, JS-thread dispatch and app freeze/resume; own separate native adapter (not yet implemented).
- **Expo Modules engineer:** validate SDK58 v2 beta Swift/Kotlin and Linux/QNX JSI boundaries independently; own separate PR.
- **QNX systems engineer:** CMake QNX SDP 8.0.5, Pi/QSTI SDK, libscreen and physical hardware proof.
- **Marathon/Wayland engineer:** separate Linux/musl build, Wayland app identity, runner/sandbox/permission contracts; coordinate with upstream.
- **QA/security/performance:** ASan/UBSan, reentrancy, memory, runtime disposal, installer errors, no fake disabled permission, cold/warm frame metrics.
- **Release/documentation:** exact npm dist-tags, shared Nitro/Nitrogen/core version, lockfile, license, immutable CI artifacts, consumer example.

These are **responsibilities**, not staffed teams or endorsements. Technical work from Marc Rousavy, Callstack, Expo, QNX and Marathon maintainers is reference material; no identity or approval is implied.

## Execution / evidence order

1. Read the exact selected `SKILL.md` and record repository SHA, source path, decision changed.
2. Inspect true public API of installed Nitro/React Native/Expo and matching platform native headers.
3. Design typed API/examples/errors; reject untyped cross-platform shortcuts and no-op fallbacks.
4. Change hand-written v1 proof only within the explicitly identified short-term exception. Do not add codegen-generated output until QNX platform generator exists.
5. Exercise JS/source tests and compiler checks, then C++/Hermes native tests on host and hardware.
6. Run QNX regression independently of Linux Wayland/Marathon, then record performance/security.
7. Document any unsupported platform/API or missing device; keep PR draft.

## Skill evidence table

| Skill                | Source revision                           | Decision it influenced                                                      | Checked-in evidence                              | Skill installed/executed?                      |
| -------------------- | ----------------------------------------- | --------------------------------------------------------------------------- | ------------------------------------------------ | ---------------------------------------------- |
| api-design           | Margelo `1e9e17b`                         | explicit narrow `createQnxPlatformInfo` type and call sites                 | API_CONTRACTS.md, index.d.ts                     | source read; local CLI install not verified    |
| build-nitro-modules  | Margelo `1e9e17b`                         | block fabricated QNX Nitrogen generation, document v1 class proof exception | NITRO_V1_MIGRATION.md, API_CONTRACTS.md          | source read; local CLI install not verified    |
| cpp                  | Margelo `1e9e17b`                         | correct v1 native header ownership and include paths                        | NitroLogger.cpp, ThreadUtils.cpp, CMakeLists.txt | source read; compile not verified              |
| nitro-fetch          | Margelo `1e9e17b`                         | explicitly _not_ swap QNX networking to unimplemented Nitro Fetch           | API_CONTRACTS.md                                 | source read; native compatibility not verified |
| react-native-testing | Callstack source pending exact SKILL read | demand Hermes and physical-device tests, not just manifest checks           | migration checklist                              | not verified                                   |
| expo-module          | Expo source pending exact SKILL read      | separate Expo 2 beta JSI/Swift/Kotlin capability review                     | linked Expo PR                                   | not verified                                   |

This table distinguishes source review from a tool-driven agent skill execution, a successful test, a source-derived rule or a hypothetical platform API.
