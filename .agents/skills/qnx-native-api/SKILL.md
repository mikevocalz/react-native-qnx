---
name: qnx-native-api
description: Design, implement, and review native APIs for react-native-qnx and its Marathon Linux target. Use for Nitro bindings, C++ ownership, Qt event dispatch, QNX services or IPC, Forge integration, permissions, and runtime disposal.
---

# QNX native API workflow

Read the repository's `AGENTS.md` first. Treat these as repository instructions, not evidence that a platform or library works. Find the repository root before resolving the paths below.

## Establish the source contract

1. Read `docs/platform-modernization/SKILLS.md`, its `skills.lock.json`, and the workstream's delivery plan. Fetch and read the pinned upstream `api-design`, then `build-nitro-modules`, then `cpp` SKILL.md sources. Record which decision each source changes. Source reading, installing a skill, executing a test, and proving a device are separate statuses.
2. Inspect the actual branch, open prerequisite PRs, lockfile, generated bindings, native headers, and the target toolchain. Resolve conflicting historical documentation against those sources; preserve dated evidence instead of treating it as the current head.
3. Identify QNX versus Linux, Screen versus Wayland, and the JavaScript platform identity independently. Reuse Fabric, Hermes, Yoga, Qt Quick, and the existing platform adapters. Keep Yoga authoritative for layout.
4. Check the exact Nitro API generation. Generic upstream skill examples may describe the older HybridObject/spec workflow. This repository's audited v1 alpha uses HybridClass conventions and a constrained platform-info generator. Inspect its supported type shapes; do not invent generated QNX bindings or assume arbitrary Promise/session support.

## Design before binding

- Write the public TypeScript contract and two or three real call sites, including a failure and cleanup. Mark sketches as proposed until they typecheck and compile against the selected generator/runtime.
- Keep cheap cached local reads synchronous. Make I/O, hardware negotiation, blocking IPC, and heavy computation asynchronous on an owned execution context.
- Return ready resource handles from asynchronous factories. Define ownership, idempotent disposal, cancellation, timeout, late completion, and stale-handle behavior before C++ work.
- Use real JavaScript Error failures at the public API. Keep wire error envelopes inside the transport adapter. Describe optional preferences separately from requirements.
- Use additive subscriptions with a cleanup handle and explicit initial-snapshot ordering. Do not expose a generic privileged command dispatcher to application JavaScript.
- Keep React hooks above the same imperative API. Avoid wrappers that silently change generated argument, error, or lifetime semantics.

## Implement one bounded native slice

1. Name the DRI and independent reviewer roles, exact paths, prerequisites, and smallest demonstrable behavior in the PR. Use unassigned roles until real people accept responsibility.
2. Reuse the host's completion queue when available. PR #44's audited implementation keeps JavaScript on the Qt owner thread and returns worker results through the RN RuntimeScheduler. Recheck the current source before extending it; do not install a competing dispatcher or assume a separate JS thread exists.
3. Keep Qt view mutation on the Qt owner and scene-graph operations on their permitted thread. Keep native worker state behind one owner. Use RAII; invoke user callbacks only outside locks.
4. Stop native producers before closing completion admission. Define what happens to every accepted operation. Queue draining is not cancellation, a producer deadline, or permission to destroy Hermes while JS is active. Test rejection after closure and work submitted during teardown.
5. For QNX IPC, use the selected SDP's types and headers, bounded/versioned payloads, native authorization, and explicit connection/event cleanup. Review SEND and REPLY blocking, server unblock cooperation, and side effects after caller timeout. Never execute MsgSend on the GUI/JS owner.
6. For shared buffers, require measured need. Define handles, offsets, lengths, epochs, access rights, synchronization, and lifetime. Never pass cross-process raw pointers or call a pipeline zero-copy without measuring each transfer.
7. Isolate experimental behavior behind a default-OFF switch, reject unsupported configurations, and preserve the original QNX path. Do not introduce runtime or ABI upgrades incidentally.

## Validate and hand off

Use `$qnx-platform-validation` for the evidence report. Exercise actual API behavior, denied permission, service loss, worker errors, late callbacks, reload/disposal, and memory ownership. Attribute host, cross-build, VM, QNX physical, and Marathon physical results separately. A JavaScript import, a generated file, or an isolated compiler success does not prove a native feature.

Record source hashes, exact commands, raw artifacts, measured budgets, missing runs, licensing impact, implementation dependencies, and rollback. Keep the PR draft when required runtime evidence is missing. Continue authorized documentation and isolated work when a device or vendor decision blocks only a later stage; do not ask again for authorization already granted in the session.
