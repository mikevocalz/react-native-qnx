# Workstream roster and handoff contract

These are proposed responsibilities. Human DRI and independent reviewer assignments remain **unassigned** until the repository owner or a contributor accepts them. No vendor, upstream maintainer, or named external author is represented as participating. The drafting agents are research/authorship roles, not the implementation team.

| Plan | DRI role | Independent reviewer role | Implementation path ownership | Required deliverable |
| --- | --- | --- | --- | --- |
| 00 Program and skills | Platform integrator | QNX maintainer + release reviewer | `AGENTS.md`, `.agents/skills`, `docs/platform-modernization` | Source/evidence map, skills, dependency catalog, staffed handoff |
| 01 Runtime and repeatable startup | QNX runtime engineer | Qt/JSI lifecycle reviewer | `vnext/src/runtime`, `native/platform/qnx`, `scripts/pi-*`, runtime evidence docs | Exact candidate, repeatable launch/exit and recovery, thread ownership decision |
| 02 Input and Worklets | RN input/runtime engineer | Qt input + QNX regression reviewer | `vnext/src/fabric`, `vnext/src/runtime`, platform JS overrides, gesture tests | Cancel-safe input, real Worklets/RNGH/Reanimated slices |
| 03 Frame pacing | Qt performance engineer | Independent measurement reviewer | Runtime/Fabric trace points, `tests/perf`, trace tooling | Repeatable presented-frame and input-latency measurements |
| 04 Text and accessibility | Text/input engineer | Accessibility + Qt platform reviewer | Qt TextInput/view backend, RN keyboard/accessibility adapters | Composition, focus, keyboard geometry, real accessibility behavior |
| 05 Lifecycle | Runtime lifecycle engineer | Service ownership + shell reviewer | Runtime host, platform lifecycle adapters, app state | Visibility/focus policy, retained background capabilities, safe restore |
| 06 Platform services | API architect | Security + native systems reviewer | Proposed typed service package; `native/platform/qnx`, `native/platform/linux-host` | Domain APIs and truthful capability/permission behavior |
| 07 QNX IPC | QNX systems engineer | IPC/security reviewer | QNX transport, protocol codec, native test server | Bounded asynchronous transport, timeout/unblock/reconnect semantics |
| 08 Service recovery | QNX reliability engineer | Lifecycle + security reviewer | Native supervisors/adapters and recovery fixtures | Failure isolation, reconnection, generations and bounded shutdown |
| 09 Shared buffers | Native memory/media engineer | C++ concurrency + security reviewer | Shared-buffer ownership/transport and stress fixtures | Measured copy-path benefit, lifetime and backpressure proof |
| 10 Forge bindings | Forge/Nitro engineer | Rust ownership + JSI reviewer | QNX Forge host, native module bindings, generated adapter | One ABI, typed existing contracts, one owner route, real editor interaction |
| 11 Device profiles | Platform capability engineer | Layout/input + hardware QA reviewer | Platform metrics providers, device profiles and fixtures | Measured display/input capabilities, adaptive and foldable behavior |
| 12 Conformance app | RN application/QA engineer | QNX + Marathon runtime reviewers | `apps/`, integration/performance fixtures, evidence manifests | Same substantive app on both targets with separately labeled results |
| 13 Marathon client/runner | Marathon integration engineer | Shell security + packaging reviewer | Fork RFC, app runner/schema/security/lifecycle paths | Reviewed native launch contract then managed packaging contract |

Paths are ownership boundaries, not declarations that all proposed packages or tests exist. Each delivery document refines exact existing versus proposed paths. Runtime/Fabric files shared by several plans require the integrator to sequence implementation PRs and resolve ownership before concurrent edits.

## Review and dependency rules

Documentation PRs can be reviewed independently. Implementation prerequisites are substantive: do not infer that merging a plan provides an API or runtime. The catalog separates existing implementation PRs from new delivery plans. A reviewer signs only the evidence they actually inspected. Assign at least one reviewer independent of the implementation author; automated drafting review is not a vendor approval.

Before starting a slice, hand off: goal, current base/head, exact paths, source/skill pins, public contract, expected observable result, feature switch, predecessor implementation, validation commands, required target, rollback and unresolved decisions. Afterward attach actual outputs, artifact hashes, failures, measured budgets and the next slice. Do not check future acceptance boxes or assign a person merely because a roster mentions their expertise.

## Existing work to preserve

PR #44 owns the physical Pi runtime and Qt/Nitro completion queue baseline. PR #46 owns the input cancellation experiment. PR #45 owns the animation integration audit. PR #42 owns the shared Rust QNX host experiment. New delivery plans reference those changes; they do not supersede their review, duplicate their queues, or declare their unverified scopes complete.
