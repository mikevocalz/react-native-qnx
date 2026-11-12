# React Native QNX: Expo delivery phases

Keep **React Native QNX** as the name and QNX as the primary/default target. Deliver one ordinary Expo application with an additional native MarathonOS target, reusing portable Qt/Fabric/Hermes source and separate operating-system adapters. QNX builds and releases must not depend on Marathon services or approval. Forge remains a separate optional premium product.

This is the product delivery layer over the [existing native workstreams](../platform-modernization/README.md), not another implementation or replacement renderer. All eight PRs below contain committed planning documents, path ownership, DRI/reviewer roles, skill sources, implementation slices, failure tests, target evidence gates and rollback. Human assignments remain unassigned. Planning approval is not implementation or release acceptance.

## Phase PRs

- Phase 0: [#81 - reviewed integration, roster and skill governance](https://github.com/mikevocalz/react-native-qnx/pull/81).
- Phase 1: [#82 - external Expo app, public imports and full QNX launch](https://github.com/mikevocalz/react-native-qnx/pull/82).
- Phase 2: [#83 - native package generation, registration and execution](https://github.com/mikevocalz/react-native-qnx/pull/83).
- Phase 3: [#84 - Router, native input, gestures and accessibility](https://github.com/mikevocalz/react-native-qnx/pull/84).
- Phase 4: [#85 - typed services, permissions, lifecycle and restoration](https://github.com/mikevocalz/react-native-qnx/pull/85).
- Phase 5: [#86 - Marathon native client and managed packaging](https://github.com/mikevocalz/react-native-qnx/pull/86).
- Phase 6: [#87 - consumer tools, reload, debugging and package delivery](https://github.com/mikevocalz/react-native-qnx/pull/87).
- Phase 7: [#88 - physical qualification and versioned QNX release](https://github.com/mikevocalz/react-native-qnx/pull/88).

Each is an independent planning branch against main `cfc28165525e0d7c54192daf58b9b4318a26e83a`. Phase files are added by their own PRs, so links use PRs rather than pretending all files are already on main. [catalog.json](catalog.json) records filenames, branches, dependencies, implementation status and old-workstream coverage.

## Execution order

QNX critical path: Phase0 -> Phase1 -> relevant Phase2/3/4 capabilities -> Phase6 -> Phase7. Phase2 enables generated native package contracts; Phase3 and Phase4 can progress through coordinated slices rather than waiting for every unrelated capability. Shared runtime/Fabric/CMake/workflow edits require one integrator to sequence ownership.

Phase5's toolchain/shell/packaging audit starts alongside Phase0/1. Its managed application acceptance consumes the shared app, native module, input and service contracts. Phase6's Marathon distribution path additionally depends on Phase5; QNX's path does not. Phase7 publishes independently justified target labels, rather than letting an unfinished Marathon approval block qualified QNX delivery.

No fixed calendar estimates or parallel-agent execution are implied. A roster names responsibilities; actual implementers/reviewers accept assignments before code approval. No upstream author is assigned without consent.

## Common reference application

Use one non-premium external Expo app and grow it through the phases: public imports, app identity, shared Router tree, substantial list, forms, interrupted drawer, persistence, native async service, permission errors and lifecycle restoration. App state uses Zustand. Target configuration can differ; duplicating screens to hide platform gaps cannot satisfy the goal. NYC-Tron UI may be tested as a consuming library. Core build/CI/release must work without Forge.

## Skills and proof

The new [qnx-expo-phase-delivery skill](../../.agents/skills/qnx-expo-phase-delivery/SKILL.md) coordinates this series. It complements, not replaces, [qnx-native-api](../../.agents/skills/qnx-native-api/SKILL.md), [qnx-platform-validation](../../.agents/skills/qnx-platform-validation/SKILL.md), the [native v1 policy](../platform-modernization/NITRO_V1_API_POLICY.md) and existing [roster](../platform-modernization/ROSTER.md).

[skills.lock.json](skills.lock.json) records immutable source commits/paths and observed Git blob identities for the core guidance read during planning. Mandatory native API order: Margelo api-design -> build-nitro-modules -> cpp. Apply actual Nitro v1 source contracts, not old generic examples. Expo API2 compatibility is separate from npm major numbers and from Swift/Kotlin-only examples. Use Callstack library/CI/performance/docs skills only for their scopes. Conditional specialist skills must be discovered, pinned and read before implementation; names are not execution evidence.

Each implementation report includes source/skill revision, local installation/read status, decision changed, exact paths, commands, logs/hashes, independent reviewer, performance boundary, security/licensing impact and rollback. Host/Wayland/QNX cross-build/VM/physical/Marathon shell results stay distinct. Screenshots do not prove interaction or presentation; discovery does not prove module registration; tunnels do not prove debugger attachment.

## Existing implementation and release records

Preserve PRs74-79 and [integration PR80](https://github.com/mikevocalz/react-native-qnx/pull/80) as implementation prerequisites. These planning PRs neither merge them nor inherit their checks. Preserve [Marathon fork PR1](https://github.com/mikevocalz/Marathon-Shell/pull/1) as the companion shell-contract RFC; future implementation belongs in reviewed client/fork slices.

Use the [v1 gates](../releases/v1-readiness.json), [component matrix](../component-support.md), [source handoff](../handoff/CURRENT_STATE.md) and [eligible open-source Qt decision](../adr/0013-open-source-qt.md). No readiness status is changed by this series. No runtime, dependency version, lockfile, recovery boot, package publication or premium-product contents are changed.
