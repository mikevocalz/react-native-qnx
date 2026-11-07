# Phase 0: QNX-first integration and delivery governance

Status: proposed delivery plan; documentation only. Implementation and acceptance remain open.
Planning source: main `cfc28165525e0d7c54192daf58b9b4318a26e83a`, inspected 2026-11-07.

## Product contract

Keep the name **React Native QNX**, repository `react-native-qnx`, and QNX as the primary/default target. Deliver one ordinary Expo application outside the platform repository, shared application source and public imports, with an additional native MarathonOS target. QNX builds must not require Marathon tools or services. Reuse Hermes, JSI, Fabric, Yoga and Qt Quick; Yoga remains the layout authority. Share portable source, not QNX/Linux executable binaries. No second renderer, browser shell, WebView or Android emulation.

NYC-Tron UI is an optional consuming application library. Nitro Forge is a separately licensed premium product, not an OS component, core dependency, default sample or CI/release prerequisite. Generic linking/lifecycle hooks belong here; product bindings and entitlements stay with the product or licensed app.

## Existing evidence and work to preserve

Read [AGENTS](../../AGENTS.md), [handoff](../handoff/CURRENT_STATE.md), [component support](../component-support.md), [Marathon architecture](../marathon/ARCHITECTURE.md), [existing program](../platform-modernization/README.md), [Nitro policy](../platform-modernization/NITRO_V1_API_POLICY.md) and [Qt decision](../adr/0013-open-source-qt.md). These documents contain dated evidence; do not substitute their old status text for candidate-specific results.

PRs [74](https://github.com/mikevocalz/react-native-qnx/pull/74), [75](https://github.com/mikevocalz/react-native-qnx/pull/75), [76](https://github.com/mikevocalz/react-native-qnx/pull/76), [77](https://github.com/mikevocalz/react-native-qnx/pull/77), [78](https://github.com/mikevocalz/react-native-qnx/pull/78), [79](https://github.com/mikevocalz/react-native-qnx/pull/79) and integration [80](https://github.com/mikevocalz/react-native-qnx/pull/80) are implementation prerequisites to review, not features to rebuild. PR80's inspected head is `43f0692aabea57e0f965b892858811349db8ec5a`. Preserve constituent histories, both command scripts, both workload-capture steps and the fresh-source workflow. A provisioned-host checkout test is not a clean-machine QNX deployment.

Reuse old workstreams 00/01/10/12/15 for governance, runtime, packages, conformance and source qualification. The eight delivery phases organize product milestones above the existing workstreams; they do not replace their native designs or close them automatically.

## Roster and path ownership

Human DRIs and independent reviewers are **unassigned**. These are staffing slots, not claims that subagents or vendors performed work.

- Platform integrator: owns candidate selection, merge ordering, `docs/expo-delivery/`, `docs/releases/` and shared-file scheduling. Independent reviewer: QNX maintainer/release engineer. Deliverable: exact source ledger and one nonduplicating integration route.
- Dependency/ABI engineer: owns `package.json`, package manifests, `pnpm-lock.yaml` and dependency validators. Reviewer: Hermes/Nitro engineer. Deliverable: matched runtime, generator, compiler and package matrix.
- QNX runtime owner: owns `native/platform/qnx/`, QNX CMake presets and recovery runbooks. Reviewer: Qt/JSI lifetime engineer. Deliverable: preserved recovery path and named release-board contract.
- Marathon integration owner: owns `docs/marathon/`, Linux adapter boundaries and companion fork contracts. Reviewer: shell/security engineer. Deliverable: early native-client and managed-app decision, without making QNX dependent on it.
- QA/evidence owner: owns `tests/`, workflow evidence and support records. Reviewer: independent measurement reviewer. Deliverable: candidate-specific results and no inherited or skipped passes.
- Developer-experience owner: owns the common reference-app requirements and versioned support language. Reviewer: an engineer not involved in implementing the workflow. Deliverable: documented acceptance from a consumer's perspective.

Only the integrator schedules concurrent edits to `CMakeLists.txt`, shared runtime/Fabric files, root scripts and workflow matrices. Separate contributors may own nonoverlapping slices; all handoffs name paths, predecessor contracts and rollback.

## Pinned skill requirements

Planning source guidance was read; external installation and native execution are not claimed. Before implementation, install/read the relevant exact sources using the team's repository-scoped mechanism and record the source revision, local path, applied decision and test artifact.

- Repository `qnx-native-api` and `qnx-platform-validation`: use the files under `.agents/skills/` at this plan's source commit; re-read with the implementation head.
- Margelo `api-design` -> `build-nitro-modules` -> `cpp`, in that order, at `1e9e17be6f41c838db472d6a4b943fc26355a3be`: [API](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/api-design/SKILL.md), [Nitro](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/build-nitro-modules/SKILL.md), [C++](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/cpp/SKILL.md). Required for any subsequent native API slice; governance alone does not execute that pipeline.
- Callstack `react-native-best-practices`, `github-actions`, `create-react-native-library`, `writing-user-docs`: source `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`, under `skills/<name>/SKILL.md`. Apply measured behavior, exact-run artifacts and package boundaries. User-doc guidance is for later consumer guides, not this internal plan.
- Expo `expo-module`: [exact source](https://github.com/expo/skills/blob/a9e5bd3fa00b4e61f632e43f85da73aee1942582/plugins/expo/skills/expo-module/SKILL.md). Apply explicit platform declarations, not assumed Swift/Kotlin portability. Read API-2 migration guidance only for an actual affected migration, after verifying its exact path and revision.

The reviewed Nitro core/adapter/generator baseline is `1.0.0-alpha.nightly.105.g69d365fc9b18`, not an assertion of the latest release. Follow actual v1 class/HybridClass/getHybridClass/constructor contracts. Do not copy older createHybridObject examples into the new v1 path. Upgrades require a separate compatibility change, regenerated lockfile, headers, generated output and runtime tests. Do not invent QNX generator metadata.

## Implementation slices

1. Inventory all prerequisite heads, diffs, checks and exact-run artifacts; distinguish planning PRs from implementation. Choose either the reviewed integration route or reviewed constituent merges, never both without reconciliation.
2. Freeze the runtime/toolchain matrix and name the initial QNX board/image and development hosts. Keep stock QSTI and custom Pi400 separate. Refresh source-contract checks before changing APIs.
3. Establish one non-premium external Expo reference app: routes, list, form, gesture drawer, persistence and one async native service added progressively. App state uses Zustand; no copied target-specific screens.
4. Publish a dependency ledger and actual staffing assignments. Start Phase5's shell-contract audit early; runtime completion follows the shared contracts rather than blocking QNX-first delivery.

## Dependencies and exit gate

No new phase prerequisite. Phase1 consumes this reviewed baseline. Phases2-4 branch by ownership after Phase1's app contract. Phase5 can audit early but integrates after shared module/UI/service contracts. Phase6's QNX workflow depends on Phases1-4; Marathon packaging additionally depends on Phase5. Phase7 may qualify QNX independently of Marathon's support label.

- [ ] One reviewed integration route; no duplicate implementation or silent feature loss.
- [ ] Exact source, lockfile, native flags, board/image and artifact ledger.
- [ ] Independent reviewers assigned before implementation approval.
- [ ] QNX-only build has no Marathon or premium dependency.
- [ ] Public support claims distinguish source generation, host execution, cross-build and physical results.

## Validation, risks and rollback

For this planning change, validate Markdown references, roster coverage, skill pins, the acyclic phase dependency graph and all eight phase identities. Native/runtime/physical tests are not run or required to claim a documentation-only change. Implementation PRs run the existing frozen-install, typecheck, JS, dependency, architecture, codegen and relevant native ON/OFF gates, then attach target receipts. Do not rewrite readiness gates from a green host job.

No package, native source, boot setting or release artifact changes here. Preserve ADR0013's eligible open-source Qt route and inspect exact component terms before distribution; the repository's MIT license does not cover third-party payloads. No vendor endorsement or hard-real-time JavaScript claim.

Rollback of this plan removes its documentation/skill pointers only. Runtime rollback stays with the reviewed implementation commits. The next smallest delivery is Phase1's external public-import app, not another platform-only screenshot.
