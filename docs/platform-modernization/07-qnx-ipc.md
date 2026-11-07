# 07 — Bounded QNX service IPC for native APIs

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Status and architecture boundary

This PR adds a **documentation-only implementation plan**. It does not add a QNX daemon, transport, deadline guarantee, or working service module. The intended outcome is a small native transport beneath typed service APIs from Plan 06. Applications continue to use Nitro domain objects; they do not receive an unrestricted `sendMessage(service, bytes)` capability.

Keep Fabric mounting and Qt Quick rendering in their existing process. Use IPC for services that already live across a trust/process boundary or need independent recovery. A microkernel does not require moving every React Native operation into a separate process.

The reviewed base is [`e84b51430c239bc89dbd48ac35afab89f7c77c01`](https://github.com/mikevocalz/react-native-qnx/tree/e84b51430c239bc89dbd48ac35afab89f7c77c01). [#44](https://github.com/mikevocalz/react-native-qnx/pull/44), at `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, supplies the owned Qt/Nitro completion path and physical Pi 400 evidence. JavaScript shares its Qt owner thread, so blocking `MsgSend` belongs on a bounded native worker, with completion routed through that existing dispatcher.

## What the Neutrino archive contributes

The user's reference is pinned at [`b1fd3e9410c1a6a0d7217c3be7e324e900470d2a`](https://github.com/Himmele/My-Blog-Repository/tree/b1fd3e9410c1a6a0d7217c3be7e324e900470d2a/QNX%20Neutrino%20RTOS/Programming). Its examples are useful for learning channels, connections, Send/Receive/Reply, timers, and resource managers. They are not the production implementation template.

The inspected [`TimerTimeout.c`](https://github.com/Himmele/My-Blog-Repository/blob/b1fd3e9410c1a6a0d7217c3be7e324e900470d2a/QNX%20Neutrino%20RTOS/Programming/Neutrino/TimerTimeout.c) times out the **receiver**, uses `CLOCK_REALTIME`, and uses sleeps around directly created threads. It does not demonstrate a bounded client RPC. Rebuild the lesson as a current C++20 fixture with explicit readiness, RAII, and observable failures; do not vendor archived binaries or retrofit that sample into the runtime.

The [QNX 8 migration guide](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.migration/topic/kernel.html) requires 64-bit `rcvid_t` for receive identifiers, registered cross-process events, and a preallocated stack for direct `ThreadCreate`; `pthread_create` handles its own stack allocation. Use installed SDP headers as the compile authority. Do not copy removed interrupt-handler APIs or change the system clock period to improve UI responsiveness.

## Proposed transport contract

Create a transport owner that holds connections, a bounded request queue, cancellation state, event registrations, and a fixed number of workers. A first proof can use one worker per service and a maximum of 16 admitted requests, including queued work; these are **proposed limits**, configurable only within documented upper bounds. Excess admission fails promptly with a typed busy error. Never spawn an unbounded replacement worker when a service hangs.

Define an explicit wire codec rather than sending in-memory C++ objects. Proposed framing:

| Field | Required semantics |
| --- | --- |
| Protocol version and header size | Negotiate supported versions; reject unknown required fields |
| Operation and payload size | Allowlisted operations; validate actual received bytes and claimed lengths |
| Request ID and service generation | Correlate exactly once; reject late responses from an old connection |
| Status and reply size | Distinguish transport failure from service refusal; bound both directions |
| Payload | Fixed-width scalar encoding, explicit byte order, checked offsets; no pointers or native `size_t` |

Start with a proposed 64 KiB maximum message, including its header. Larger data requires the separately justified Plan 09 path. Validate lengths before allocation and check subtraction/addition for overflow. Treat pulses, protocol messages, connection handling, and receive errors as distinct native paths. Store receive IDs as `rcvid_t` throughout deferred replies and unblock bookkeeping.

A request owns its send and reply buffers until the native call has actually finished. Caller cancellation or a rejected JS Promise does not permit freeing buffers still used by a blocked worker. Wire failure envelopes may contain codes and messages; reconstruct real JavaScript `Error` objects at the Nitro boundary.

## Deadlines, cancellation, and shutdown

Record the deadline against a monotonic clock at admission, so queue time counts. Reject expired queued work before sending. Immediately before `MsgSend*`, arm `TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, ...)` for the remaining budget, with no intervening kernel call. QNX documents that another kernel call can disarm the timeout. See [TimerTimeout](https://qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/t/timertimeout.html).

The same documentation makes an important qualification: once a server using `_NTO_CHF_UNBLOCK` has received the request, it must handle the unblock pulse and reply or error. A timeout setting alone is not an unconditional resource-reclamation or end-to-end deadline guarantee. Test that server behavior explicitly. A caller-visible timeout must be distinguished from “server operation never happened.” Do not retry a mutation unless the protocol provides durable deduplication or otherwise proves it is safe.

For services whose termination/unblock behavior cannot be bounded, stop admission and quarantine the connection. Prove a contained broker-process design with Plan 08 before admitting such calls into an in-process worker. Do not detach a blocked thread and call teardown complete. Do not use asynchronous thread cancellation as an unreviewed shortcut.

Shutdown closes admission, removes subscriptions, requests cooperative cancellation, finishes native ownership cleanup, and only then permits #44's host drain and Hermes destruction. No worker may synchronously wait for JavaScript while the owner waits for that worker. Freeze/resume follows Plan 05 and must not masquerade as a transport timeout policy.

## Proposed call examples

These are **native contract pseudocode**, not compiled APIs or generated bindings. `SnapshotResult` is a C++20 variant of a validated snapshot or a transport failure; the service adapter forwards its completion through #44's existing queue.

```cpp
// A read operation; callback ownership belongs to the service session.
auto request = client.requestSnapshot(
    RequestOptions{.timeoutMs = 250},
    [session](SnapshotResult result) { session->completeSnapshot(result); });
```

```cpp
// Closing a screen requests cancellation; the transport still owns buffers
// until MsgSend returns. A concurrent reply must settle only once.
request.cancel();
```

```cpp
// Disposal starts on the native session's owner. Completion is asynchronous.
client.close([session](CloseResult result) {
  session->finishDisposal(result); // Error if native ownership is still live.
});
```

The implementation must break retained callback/session cycles and define what happens when the completion queue has closed. It cannot silently discard an accepted Promise settlement. The 250 ms value is a fixture budget for testing; it is not a promised product latency.

## Registered events and security

For notifications, register the event with `MsgRegisterEvent` against the connection to the authorized server, send the returned handle, and unregister during teardown. A pulse's delivery connection differs from the server connection authorizing delivery; preserve that distinction. Do not use `_NTO_REGEVENT_ALLOW_ANY_SERVER`. See [MsgRegisterEvent](https://qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/m/msgregisterevent.html).

Prefer a reviewed resource manager or existing service when filesystem/credential semantics are appropriate. Validate native caller authorization for each operation, not only at JavaScript entry or initial connection. Follow QNX's [resource-manager security guidance](https://qnx.com/developers/docs/8.0/com.qnx.doc.security.system/topic/manual/write_resource_manager.html) for permissions, message sizes, and offsets. A client-supplied app ID is not authority. Logging must omit message bodies, credentials, and personal data by default.

## Work slices, roster, and skills

| Slice / owner role | Proposed files and deliverable | Independent reviewer |
| --- | --- | --- |
| Protocol engineer, DRI | New `native/platform/qnx/services/ipc/` codec and version contract | Security engineer |
| QNX systems engineer | Connection/event RAII, cooperative server fixture, bounded worker | Qt/JSI lifetime engineer |
| Nitro API engineer | Plan 06 session integration under `packages/qnx-nitro-runtime/cpp/` | API architect |
| QA engineer | Proposed `tests/native/qnx-ipc/` process fixtures and malformed-input tests | QNX regression owner |

All roles are proposed and human assignments are open. Implement codec and native process fixtures first, then cancellation/events, then the typed service adapter. A proposed `RNQNX_ENABLE_SERVICE_IPC` flag defaults OFF and rejects non-QNX production builds. Host fakes belong only to test targets. Plans 01/#44 and 06 are prerequisites for integration; Plan 08 covers recovery; Plan 09 must not be bundled into the first transport slice.

Exact skills read, in order: Margelo [`api-design`](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/api-design/SKILL.md), [`build-nitro-modules`](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/build-nitro-modules/SKILL.md), and [`cpp`](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/cpp/SKILL.md), all at `1e9e17be6f41c838db472d6a4b943fc26355a3be`; Callstack [`react-native-best-practices`](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md), at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`.

| Skill | Decision applied | Evidence limit |
| --- | --- | --- |
| `api-design` | Typed domain API; visible errors and asynchronous remote work | Source read; API sketches uncompiled |
| `build-nitro-modules` | One owned session; no fabricated Promise codegen | Source read; existing generator remains limited |
| `cpp` | RAII buffers/connections and explicit cancellation lifetime | Source read; no new native tests run |
| `react-native-best-practices` | Measure queue/owner delay and bounded memory before tuning | Source read; no latency results |

## Acceptance, evidence, and rollback

Require malformed/truncated/oversized messages, arithmetic overflow, unsupported version, denied caller, queue saturation, absent server, disconnect before/after receive, late replies, and cancel/reply/teardown races. Exercise timeout in both SEND and REPLY states, cooperative unblock handling, and a deliberately noncooperative server in an externally bounded test process. Assert one settlement per accepted request, bounded memory/workers, no owner-thread blocking, and no work entering destroyed Hermes.

Record queue wait, server time, completion delay, p50/p95/p99 latency, maximum admitted work, and resource counts before/after repeated cycles. Thread priority and affinity changes require target traces and review; do not import Marathon's Linux scheduler constants or claim JavaScript hard-real-time behavior.

Verified existing command entry points are `pnpm validate:dependencies`, `pnpm typecheck`, `pnpm test:js`, `pnpm test:cpp:host`, `pnpm validate:qnx-nitro`, and `pnpm qnx:validate:805`. They do not yet contain the proposed IPC fixtures. Add actual CMake/CTest targets and publish the exact configure/build/run commands in the implementation PR. **No implementation test was executed for this plan.** Preserve separate host, QNX VM, physical QNX, and Marathon rows; Marathon uses its own adapter and must still pass shared-service regressions.

This plan imports no archived source or proprietary SDK material. Check distribution rights for any eventual native dependency. Roll back by closing service sessions, disabling the new flag, and restoring the previous capability report. Keep the old renderer and #44 dispatcher. The next PR should prove one bounded read/reply with its timeout and denied-caller cases on QNX before expanding the operation set.
