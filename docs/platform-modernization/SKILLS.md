# Skills and native engineering requirements

This series adds two repository-scoped skills: [QNX Native API](../../.agents/skills/qnx-native-api/SKILL.md) and [QNX Platform Validation](../../.agents/skills/qnx-platform-validation/SKILL.md). They are draft repository skill definitions, not a claim of installation into a personal skill directory. Their source travels with the PR branch.

The exact upstream sources and SHA-256 of the verbatim upstream SKILL.md files are recorded in [skills.lock.json](skills.lock.json). The source snapshots were retrieved and read for this design review. No `npx skills` installation or native implementation test is implied.

Read the [Nitro v1 API-first contract](NITRO_V1_API_POLICY.md) before native implementation. The owner explicitly requires the new v1 APIs throughout the program.

## Ordered native skill pipeline

| Order | Source skill                            | Pinned source revision                     | Decision applied                                                                                            |
| ----- | --------------------------------------- | ------------------------------------------ | ----------------------------------------------------------------------------------------------------------- |
| 1     | Margelo `api-design`                    | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Contract and caller examples precede native code; errors and cleanup are observable API semantics.          |
| 2     | Margelo `build-nitro-modules`           | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Native state owns its lifetime; generated bindings need real supported generator output and behavior tests. |
| 3     | Margelo `cpp`                           | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | RAII, a clear owner queue, no callbacks under locks, and no synchronous worker wait on the GUI/JS owner.    |
| 4     | Callstack `react-native-best-practices` | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Measure the actual interaction before optimizing; separate JS, native, and presented-frame evidence.        |

Read the source in that order for native API work. Record an actual decision, not only a skill name. Re-audit the applicable references before implementation; a source snapshot is reproducible guidance, not permission to apply stale APIs.

The Nitro skill includes older `.nitro.ts`/HybridObject and Apple/Android examples. The inspected repository pins Nitro core, adapter and Nitrogen to `1.0.0-alpha.nightly.136.gdb81de61bd32`; inspect its v1 HybridClass conventions. `scripts/nitro-platform-codegen.mjs` already generates the constrained platform-info/getter/add proof. That is not general upstream QNX/Marathon Nitrogen support. Promise, subscription, object graph, callback and Forge generation must be demonstrated. Do not remove the limited adapter or invent generated output to satisfy a generic skill example.

Callstack's mobile profiling tools and `agent-device` commands do not establish QNX or Marathon device support. Use the repository's actual Qt/QNX tools and record gaps. `writing-user-docs` was read to verify scope and is not applied to these internal RFCs. Apply it when producing developer installation or user-facing guides.

## Conditional skills

| Change                          | Required selection before implementation                                                                             | Status in this series                                                        |
| ------------------------------- | -------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| RN/JSI/Hermes version migration | Callstack `upgrading-react-native`, exact source revision and installed-source audit                                 | Required if the slice changes those versions; not claimed read/applied here. |
| GitHub Actions edits            | Callstack `github-actions`, plus current workflow permissions and artifact inspection                                | No workflow edit in this series; future source-read gate.                    |
| Expo Modules bridge             | Verify the current official Expo `expo-module` skill path and release; audit QNX C++/JSI compatibility independently | Future gate; Swift/Kotlin documentation is not QNX runtime proof.            |
| HTTP/WebSocket/native decoding  | Verify Margelo `nitro-fetch` source and exact supported platform code                                                | Conditional; do not replace networking based on a skill name.                |
| Persistent native storage       | Verify Margelo `react-native-mmkv` source, version, encryption and multiprocess semantics                            | Conditional; no storage migration in this series.                            |
| Apple/Android implementation    | Margelo `swift` or `kotlin` as appropriate                                                                           | Not selected for a QNX/Linux C++ scope.                                      |

Use the existing repository roster and the [workstream roster](ROSTER.md) together. Upstream authors and vendors are sources, not assigned contributors or reviewers.

## Required evidence row in implementation PRs

Record exact skill name, repository SHA/path, source-read status, local installation status where relevant, the changed decision, actual implementation path, test command/artifact and remaining device gap. Preserve `not run` and `blocked` as explicit outcomes. Apply `$qnx-native-api` to the implementation and `$qnx-platform-validation` to its report; neither skill substitutes for independent review or physical execution.
