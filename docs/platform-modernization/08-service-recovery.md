# 08 — Native service recovery and bounded runtime shutdown

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Status and intended result

This is a **documentation-only plan**, with no supervisor, restart policy, daemon configuration, or fault-injection result added. It defines how the application remains usable when a native provider stops, hangs, restarts, loses permission, or becomes incompatible. Implementation roles and independent reviewers are proposed, not staffed.

The UI should show an explicit unavailable/recovering state, preserve application-owned data, and resume from a fresh service snapshot when recovery succeeds. Native supervision belongs outside the JavaScript event loop. Recovery must not recreate callbacks into a disposed Hermes runtime or replay an operation with unknown side effects.

## Source inspection and existing work

Baseline: [`mikevocalz/react-native-qnx@e84b51430c239bc89dbd48ac35afab89f7c77c01`](https://github.com/mikevocalz/react-native-qnx/tree/e84b51430c239bc89dbd48ac35afab89f7c77c01). Runtime prerequisite [#44](https://github.com/mikevocalz/react-native-qnx/pull/44), inspected at `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, already proves an owned Qt/Nitro queue on physical Pi 400. Its [`dispatcher contract`](https://github.com/mikevocalz/react-native-qnx/blob/e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d/docs/nitro/QT_DISPATCHER.md) drains accepted and transitive completions before Hermes destruction, but explicitly does not provide producer cancellation or a deadline. A non-returning accepted task or owner-thread task continually posting transitive settlement work can delay that drain; closed worker admission rejects later worker posts. This plan fills the producer/service side of that contract rather than adding another dispatcher.

QNX SDP 8.0.5's [release notes](https://www.qnx.com/developers/docs/relnotes8.0/com.qnx.doc.release_notes/topic/8.0.5_prod_release_notes/8.0.5_rn.html) introduce the Process Launcher and Monitor Service. Evaluate [PLMS/QPLMS](https://qnx.com/developers/docs/8.0/com.qnx.doc.plms/topic/plms_overview.html) first for dependency-based startup, process monitoring and recovery before writing an overlapping supervisor. Confirm the exact package, BSP integration, configuration, abilities and redistribution scope on the actual image. QNX's [High Availability Framework](https://qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.prog/topic/process_High_Availability.html) and a narrowly scoped child supervisor are alternatives to compare against that evidence; none is assumed deployed by this plan. Do not infer certification of React Native, Hermes, the app or its configuration from a separately described QNX component.

The [PLMS binary contract](https://qnx.com/developers/docs/8.0/com.qnx.doc.plms/topic/plms_binary.html) matters to the test design: dry-run parsing does not validate files reached through `load_config`, and launch-and-exit mode does not monitor/recover processes. Inventory the complete configuration dependency graph, test the actual monitor mode, and keep the debug utility out of the public app API. Verify least-privilege [abilities](https://qnx.com/developers/docs/8.0/com.qnx.doc.plms/topic/abilities.html) and terminal recovery actions in an isolated fixture. No automatic system reboot or graphics boot change belongs in this application-service test.

Marathon source was inspected at [`c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5`](https://github.com/patrickjquinn/Marathon-Shell/tree/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5). Its [`ipc.rs`](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/crates/marathon-shell-native/src/ipc.rs) separates asynchronous service work from GUI ownership. Its [native lifecycle contract](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/NATIVE_APP_LIFECYCLE.md) includes freeze/thaw and service interactions. Those lessons support explicit state transitions; Linux cgroups and systemd operations are not QNX implementations.

## Recovery model

Keep three independent identities: the application runtime generation, the current service instance, and each request. Never treat a reused PID or bus name as proof that an old connection is valid. Native callbacks carry a session/runtime token that is checked before scheduling into the existing completion queue.

The transport/session state machine is proposed as:

| State      | Admission and transition rule                                                               |
| ---------- | ------------------------------------------------------------------------------------------- |
| Connecting | Establish authenticated peer and compatible protocol; no normal operation admitted          |
| Ready      | Fresh snapshot installed and observation stream reconciled; admit bounded work              |
| Recovering | Reject new mutations; expose availability, bound retained work, attempt approved reconnects |
| Failed     | Report terminal reason; no retry for denied permission or unsupported protocol              |
| Closing    | Stop admission and producers; reclaim native ownership asynchronously                       |
| Closed     | No new native work; future method calls fail with a disposed-resource Error                 |

Subscribe and snapshot with a service-defined sequence/generation handshake. Either return snapshot plus stream boundary atomically, or buffer events and reconcile against a versioned snapshot before declaring Ready. A bare “subscribe, then get state” is not sufficient without ordering semantics. On reconnect, replace the old baseline; do not apply stale deltas from an earlier service instance.

Retry only operations documented as safe to repeat. Read-only snapshot retrieval can be repeated after authentication and version negotiation. A mutation whose reply was lost has an **unknown outcome** until the service can reconcile a durable operation ID; it must not be silently repeated. Transport delivery does not imply exactly-once business execution.

Backoff is justified for external service failure, not as a workaround for broken initialization. The first policy proposes at most three automatic reconnect attempts in a ten-second window, after which the session reports Failed. These are reviewable test defaults, not measured availability targets. Reset the budget only after a documented healthy interval; stopping/disposal cancels pending attempts. Permission revocation immediately removes sensitive subscriptions and requires a new authorized workflow.

## Native supervision and shutdown

The native supervisor may restart only allowlisted processes it is explicitly responsible for. Bind that responsibility to the launched process identity and executable/configuration provenance. It must not accept arbitrary executable paths or process IDs from JavaScript, restart unrelated system daemons, or modify graphics boot policy. Plan 01 owns startup/recovery changes separately.

A heartbeat should report meaningful service progress and include freshness, not merely prove that a timer thread can tick while the worker is deadlocked. Distinguish a stalled service from a slow Qt completion queue using separate timestamps. JavaScript health observations can assist diagnostics but cannot be the only watchdog for JavaScript itself.

Graceful shutdown is ordered:

1. Close service/session admission and publish disposal state while the runtime is alive.
2. Stop subscriptions and producers, cancel bounded retries, and request cooperative native cancellation.
3. Finish or explicitly contain in-flight operations, retaining buffers until their native ownership ends.
4. Complete binding cleanup at an idle owner boundary; then allow #44's host drain and Hermes destruction.

Plan 07's noncooperative-server case cannot be solved by falsely marking a worker closed. If native waits cannot be bounded, isolate them behind a supervised broker and prove its termination/reclamation behavior before shipping that provider. Do not synchronously join a worker that needs a JS callback from the joining owner. Active-JS/reentrant host destruction remains invalid.

## Proposed application behavior

The Plan 06 monitor gains a focused availability observation. This is a **proposed API extension**, not a generated/exported implementation:

```ts
type ServiceAvailability = 'available' | 'recovering' | 'unavailable';
// On NetworkMonitor; registration returns an idempotent cleanup handle.
addOnAvailabilityChangedListener(
  listener: (state: ServiceAvailability) => void,
): ListenerSubscription;
```

The callback receives initial state asynchronously through the same ordered observation path as later changes. Read the latest owner-cached state when the initial delivery runs, and suppress older generation/sequence deliveries internally; a captured initial snapshot must never arrive after a newer state and regress the consumer. Registration, recovery and removal races have explicit tests. A recovered session keeps its public identity only if the same baseline capability/authorization contract still holds; otherwise it reports unavailable and the caller creates a new resource. Availability does not turn failed operations into successes.

**Display loss and recovery:**

```ts
const monitor = await Network.createMonitor();
const availability = monitor.addOnAvailabilityChangedListener(showServiceState);
const snapshots = monitor.addOnSnapshotChangedListener(renderNetwork);
const errors = monitor.addOnErrorListener(showNetworkError);
async function closePanel() {
  availability.remove();
  snapshots.remove();
  errors.remove();
  await monitor.dispose();
}
```

**Handle a request during an outage:**

```ts
try {
  renderNetwork(await monitor.getSnapshot());
} catch (error) {
  showNetworkError(error instanceof Error ? error : new Error(String(error)));
  // The UI may offer a new read; it does not replay an unknown mutation.
}
```

These examples use injected UI callbacks and the proposed Plan 06 root. They demonstrate reviewable error/cleanup behavior and do not assert that the package already exports these methods.

## Delivery slices and roster

| Slice                          | Proposed owner and files                                                                 | Independent reviewer             |
| ------------------------------ | ---------------------------------------------------------------------------------------- | -------------------------------- |
| Session recovery state machine | Native API DRI; focused files under `packages/qnx-nitro-runtime/cpp/`                    | Qt/JSI lifetime engineer         |
| QNX native supervision adapter | QNX systems engineer; proposed `native/platform/qnx/services/supervision/`               | Security/platform integrator     |
| Marathon reconnect adapter     | Marathon engineer; proposed `native/platform/linux-host/services/`                       | Shell integration reviewer       |
| Fault fixtures and evidence    | QA/performance engineer; proposed service process fixtures and `docs/evidence/` receipts | Independent QNX regression owner |

First implement reconnecting read-only sessions with a deterministic fixture. Next add one explicitly owned QNX service supervisor; compare PLMS, HAM and the minimal child-owner option against actual target availability before selecting one. Then prove production-backend fault recovery and UI behavior. Use a proposed `RNQNX_ENABLE_SERVICE_RECOVERY` flag, default OFF, with target checks. Do not auto-enable restart merely because the IPC flag is enabled.

Dependencies: Plan 01/#44 for ownership; Plans 06–07 for typed errors, protocol generations, deadlines, and cancellation; Plan 05 for freeze/resume and checkpoints; Plan 12 for conformance. Restoration of application documents remains application/Forge work, not an implied feature of restarting a daemon.

## Skills and validation

Exact sources read: [Margelo skills](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills) at `1e9e17be6f41c838db472d6a4b943fc26355a3be`, in `api-design` → `build-nitro-modules` → `cpp` order; [Callstack `react-native-best-practices`](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md) at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`.

| Skill                         | Applied decision                                                         | Proof status                                |
| ----------------------------- | ------------------------------------------------------------------------ | ------------------------------------------- |
| `api-design`                  | Availability observation plus Error failures; no implicit mutation retry | Source reviewed; proposed types             |
| `build-nitro-modules`         | Session owns its lifecycle; callbacks reuse the existing dispatcher      | Source reviewed; native integration pending |
| `cpp`                         | Explicit state variants, resource ownership, no deadlocking owner joins  | Source reviewed; runtime tests pending      |
| `react-native-best-practices` | Measure recovery, queue delay, memory, and UI responsiveness separately  | Source reviewed; no benchmark claims        |

Add a test that enters recovery between registration and initial availability delivery, then reconnects before that queued delivery runs; the consumer must not regress to a stale state. The first fault matrix must cover clean stop, crash, blocked service work, lost reply after mutation, restart under a reused endpoint, protocol mismatch, revoked permission, repeated outage, disposal during reconnect, and suspend/resume while disconnected. Execute 100 restart/reconnect cycles in the controlled fixture, asserting one settlement per accepted request, no cross-generation callbacks, bounded retries, and return to baseline native handle/subscription counts. Track RSS separately because allocator retention is not itself proof of a resource leak.

Measure detection time, time to first fresh snapshot, failed-operation latency, queue occupancy, idle CPU, and frame/input degradation under failure. Freeze transitions need their own trace so expected suspension is not misclassified as a crash. Record approved per-device budgets before enabling a production provider; no latency or real-time guarantees are established here.

Existing baseline commands include `pnpm test:js`, `pnpm test:cpp:host`, `pnpm validate:qnx-nitro`, `pnpm qnx:validate:805`, and `pnpm qnx:runtime:validate`. Extend real native test targets for these cases and publish their exact commands, fault scripts, SHAs, and logs. **This documentation PR ran none of those implementation gates.** Require a physical QNX receipt and a separate Marathon D-Bus service-loss/permission receipt. Host-fake or QNX VM evidence cannot fill either device row.

## Security, rollout, and rollback

Keep restart privilege in the supervisor, separate from ordinary app credentials. Redact crash/health artifacts that can contain application data. Review the selected PLMS/HAM/runtime and Qt distribution terms before including components; this plan embeds no proprietary material. Keep recovery opt-in until the owner and independent reviewer approve both success and terminal-failure behavior.

Rollback first stops supervised test services and closes app sessions, then disables the feature switch and its capability advertisement. Do not leave a restart loop active after the application feature is disabled. The next PR should deliver the read-only reconnect fixture and state machine; automatic process restart follows only after that evidence is reviewable.
