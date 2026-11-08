---
name: qnx-expo-phase-delivery
description: Plan, implement and validate the eight QNX-first Expo delivery phases while retaining React Native QNX identity, shared Qt/Fabric/Hermes architecture, an additional MarathonOS target and independent evidence gates.
---

# React Native QNX Expo phase delivery

Use this repository skill for work under `docs/expo-delivery/` and the corresponding implementation PRs. Resolve paths from the repository root. This is workflow guidance, not proof of supported APIs or completed targets.

## Start with the actual phase

Read `AGENTS.md`, the selected phase plan, `docs/platform-modernization/ROSTER.md`, `docs/platform-modernization/SKILLS.md` and `docs/platform-modernization/NITRO_V1_API_POLICY.md`. Inspect the actual base/head, dependency graph, installed source and predecessor implementation. A merged plan does not implement its checkboxes. Preserve useful prerequisite implementations and their dated evidence.

Keep the name React Native QNX and repository/package identity. QNX is primary/default; MarathonOS is an additional native Linux/Wayland target. Share application source and portable Qt/Fabric/Hermes code, not executable binaries. QNX must build without Marathon tooling or services. Yoga remains the layout authority. Do not introduce a second renderer, WebView, Android emulation or premium dependency.

## Staff and divide the work

Select the phase roster's actual DRI and a different reviewer before approval. Until people accept, write unassigned rather than assigning vendors or authors. Record concrete paths, inputs, output, predecessor contract, expected failure behavior and rollback. Shared CMake, runtime, Fabric, root scripts and workflow edits need integrator ordering. Do not claim subagents were launched because a roster names agent roles.

## Apply exact skills

For a new native API, read/install the pinned Margelo `api-design`, then `build-nitro-modules`, then `cpp` sources identified by the phase. Apply repository `qnx-native-api` and use `qnx-platform-validation` for the evidence report. Audit actual v1 imports, class/registration/constructor contracts, supported codegen shapes, runtime/core/generator ABI and owned dispatcher. Older generic skill examples do not override the repository's v1 API policy.

For Expo configuration/modules, read the verified `expo-module` source and relevant references. API 2 compatibility needs actual QNX/Linux C++/JSI implementation; Swift/Kotlin examples are not platform proof. Use Callstack library, CI, performance and user-doc skills only for their corresponding tasks. Discover and pin optional navigation, storage, networking, migration, design or review skills before claiming their use. Do not invent a skill path or use an unsupported mobile automation tool as QNX evidence.

Every implementation PR records: skill name, source commit/path, source-read status, installation status, decision changed, code paths, executed commands/artifacts and missing target evidence. Source reading, installation, tests and physical execution are distinct statuses.

## Deliver behavior, not another probe

Keep one non-premium Expo reference application outside the platform checkout. Expand the same routes/screens/data/API use as phases progress. Normal public imports, per-app identity and one target-aware workflow are the goal; hard-coded playground identity or internal RN imports cannot count as consumer completion.

Specify public API call sites with failure and cleanup first. Keep cheap local observations synchronous; put blocking IPC/I/O/compute on an owned native execution context. Define ready-resource construction, subscriptions, errors, permissions, cancellation, timeout, disposal and reload. Reuse existing queues, enforce Qt affinity, and never call user code under locks. Worklets/Reanimated/RNGH must use their real integrations, not a disguised Nitro replacement.

## Validate without overstating

Use separate result rows for Linux offscreen, Linux Wayland, actual Marathon shell, QNX cross-build, VM and physical board/image. Attach the exact source SHA, lock hash, binary/bundle hashes, flags, toolchain, commands, logs and observable result. Use passed, failed, blocked, not-run and historical-prerequisite accurately. A tunnel is not debugger attachment; discovery is not native registration; a screenshot is not interaction or frame presentation.

Keep feature experiments default OFF until their target contract is verified. Preserve recovery boot; no flash/reset/boot modification as an incidental test. Core CI and release work without Forge. Optional licensed consumer checks use authorized packages without redistributing premium source or entitlements.

Planning PR acceptance is document completeness, valid sources, ownership and dependency coherence. Implementation acceptance requires the phase's actual behavior and failure tests. Release acceptance uses the existing seven QNX gates; Marathon receives a separate support label and cannot hold a qualified QNX release indefinitely. Never imply safety certification or hard-real-time JavaScript guarantees.
