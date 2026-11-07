# Phase 4: essential services, lifecycle and state restoration

Status: proposed delivery plan; documentation only. Source: main `cfc28165525e0d7c54192daf58b9b4318a26e83a`, inspected 2026-11-07.
Implementation depends on [Phase1](https://github.com/mikevocalz/react-native-qnx/pull/82) app identity and [Phase2](https://github.com/mikevocalz/react-native-qnx/pull/83) native package/runtime contracts. It can progress alongside [Phase3](https://github.com/mikevocalz/react-native-qnx/pull/84); only the relevant provider contracts are shared prerequisites.

## Outcome and scope

The shared external Expo reference app handles normal files/preferences, network operations, links, clipboard, permission disposition, window metrics and foreground/background transitions through typed application-facing APIs. Supported state survives documented restart/restore paths; native work cannot outlive its owner accidentally. Keep React Native QNX QNX-first and use a separate Marathon adapter, never two application implementations.

Reuse existing workstreams05/06/07/08/09/11 and their [catalog](../platform-modernization/README.md), [native API policy](../platform-modernization/NITRO_V1_API_POLICY.md), [runtime baseline](../handoff/CURRENT_STATE.md), [component limits](../component-support.md) and [Marathon architecture](../marathon/ARCHITECTURE.md). Keep the existing Qt/JS owner queue, runtime scheduler and service experiments; do not introduce another worker pool by habit. The [Neutrino source map](../platform-modernization/NEUTRINO_REFERENCE_MAP.md) remains a conceptual/audit source, not legacy code to vendor wholesale.

The first supported service set is explicit. Advanced camera, media, XR, robotics and specialized hardware are later prioritized modules, not implied by generic service success. Secure storage has its own verified backend/capability contract; ordinary preferences are not a substitute for secret storage.

## API contracts to design first

A service reports actual availability/authorization separately from requested preferences. Required unsupported capabilities fail clearly. Once an asynchronous factory returns a ready resource, its baseline operations are guaranteed; no half-initialized singleton or broad privileged command dispatcher.

Cached local observations stay synchronous. File I/O, network, hardware negotiation and blocking IPC use an owned asynchronous context. Additive subscriptions expose ordered initial snapshots and idempotent cleanup. Describe cancellation, timeout, loss of service, permission revocation, late completion, retry safety and post-disposal behavior. Hooks compose the same imperative API; state in the reference app stays in Zustand.

Define lifecycle transitions for visible, inactive/background, suspended/frozen, resuming and terminating states according to each actual OS provider. Quiesce hidden animations/producers as appropriate; an allowed background operation must have a native authorization/ownership claim. JavaScript cannot grant itself background privileges or rely on callbacks while the process is frozen. Persist only documented state with schema/version handling, not arbitrary native pointers or capabilities.

Metrics distinguish window/display size, scale, safe areas, keyboard occlusion, input modes and actual fold/hinge topology. Unsupported or unknown is explicit; never manufacture a hinge, zero inset or single-panel certainty from absent hardware information.

## Roster and path ownership

All human assignments remain unassigned. Each implementation slice needs its actual DRI and an independent reviewer.

- API architect: proposed typed service specs and consumer examples; reviewer: independent API/security engineer. Deliverable: user operations, units, errors, ready-resource/lifetime and capability contracts.
- QNX systems engineer: `native/platform/qnx/`, IPC/resource-manager adapters and native test services; reviewer: QNX IPC/security engineer. Deliverable: current-SDP types, bounded protocols, native authorization and reconnect behavior.
- Marathon adapter engineer: `native/platform/linux-host/`, client service/lifecycle seams; reviewer: Linux/shell security engineer. Deliverable: actual approved service access, no unrestricted bus assumption.
- Runtime/lifecycle engineer: `vnext/src/runtime/`, owned queues, shutdown and state-restoration integration; reviewer: Qt/JSI lifetime engineer. Deliverable: producer quiescence, accepted-work accounting and generation isolation.
- Persistence/network engineer: affected generic native modules and platform-specific backends; reviewer: data-security/concurrency engineer. Deliverable: scoped storage/network behavior, secret-store refusal when unavailable, bounded retries.
- Device-profile engineer: metrics providers and capability fixtures; reviewer: adaptive layout/hardware QA. Deliverable: measured window/keyboard/fold information and change notifications.
- Failure/performance QA: registered service/process tests and external app scenarios; reviewer: independent target QA. Deliverable: denied/revoked permissions, service death, lifecycle races and resource measurements.

Proposed package/test locations must be confirmed against the implementation checkout. Shared runtime edits are serialized by the integrator; Phase3 consumes metrics without owning the OS providers.

## Required skills and native source audit

Apply repository [qnx-native-api](../../.agents/skills/qnx-native-api/SKILL.md) and [qnx-platform-validation](../../.agents/skills/qnx-platform-validation/SKILL.md). Margelo's mandatory ordered `api-design` -> `build-nitro-modules` -> `cpp` sources are pinned to `1e9e17be6f41c838db472d6a4b943fc26355a3be`, under `skills/<name>/SKILL.md`. Decisions: coherent resource factories, generated v1 contracts, RAII and explicit thread affinity; no callbacks under locks or UI-thread blocking IPC.

Use the actual repository Nitro v1 API policy and matching core/adapter/Nitrogen/headers; generic older skill syntax is not permission to introduce v0 APIs. Unsupported generator shapes need reproducible generation work, not hand-edited output. Expo `expo-module` guidance at `a9e5bd3fa00b4e61f632e43f85da73aee1942582` informs explicit module registration; Expo API2 C++/JSI compatibility must be proved separately from Apple/Android macros.

Callstack `react-native-best-practices` and `github-actions` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` guide measurement and exact-run artifacts. For actual HTTP or persistent-store implementation, discover/pin/read the Margelo networking and MMKV skills and their current supported source before choosing APIs; they are conditional requirements, not claimed read/installed by this plan. Swift/Kotlin skills apply only if those real platform implementations are changed. Read official selected-SDP IPC/security and Qt ownership sources before systems changes; documentation references are source audits, not installed skills.

## Bounded implementation sequence

1. Define the initial capability/permission matrix and two real caller flows per domain, including unavailable service and cleanup. Choose the smallest app workflow, not an all-purpose service bus.
2. Implement basic data/network services through generated Phase2 contracts. Keep file paths scoped to the app, validate protocol payload sizes and use actual platform trust stores where required.
3. Add lifecycle/visibility events, checkpoint/restore and native background ownership. Test termination during a write or pending operation and preserve schema-compatible app data.
4. Add QNX IPC recovery with connection generation and deadlines. Audit SEND versus REPLY blocking and server-side effects after caller timeout; never put MsgSend on the GUI/JS owner. Do not add transparent retries to non-idempotent operations.
5. Add measured window/input/keyboard providers and optional fold topology. Qualify changes through the shared app without OS branches in ordinary screen code.
6. Integrate the Marathon side through Phase5's approved identity/service contract. QNX service implementation must remain buildable/testable with Marathon absent.

## Acceptance and evidence

- [ ] Supported operations and real unsupported/denied errors execute through the public API in the app.
- [ ] Two consumers, subscribe/remove ordering, revocation, worker errors and repeated disposal are covered.
- [ ] Service death/restart produces no stale-generation callbacks or duplicate side effects.
- [ ] Background/resume, close while frozen, reload and pending-shutdown work have documented results.
- [ ] Persistence survives its declared scenarios; missing secure storage never silently uses plaintext.
- [ ] Actual metrics drive adaptive layouts; unknown providers remain explicit.
- [ ] Native resource counts, idle work and memory are measured; QNX and Marathon receipts remain separate.

Implementation runs frozen dependencies, typecheck, JS/codegen, compiled native behavior/failure tests and relevant OFF/ON/sanitizer gates. Add finite native test services/watchdogs with captured exit status, hashes and logs. Host mocks prove protocol cases only; physical QNX and actual Marathon services each need their own evidence. Release qualification is Phase7, not an automatic side effect.

## Security, limits and rollback

Native authorization and OS isolation enforce permissions; JS flags cannot provide confinement. Preserve recovery boot and avoid root/debug bypasses. Keep credentials/private device data out of CI artifacts. No Forge dependency, premium redistribution or SDK payloads. Shared buffers require measured copy-path need and explicit ownership/access rights; this phase does not claim zero-copy or deterministic hard-real-time JS.

This plan changes no native service or target configuration. Roll back implementation by disabling the specific adapter/feature and restoring compatible generated/runtime/persisted contracts, preserving user data and documenting any irreversible migration separately. Reverting this document alone has no runtime effect.
