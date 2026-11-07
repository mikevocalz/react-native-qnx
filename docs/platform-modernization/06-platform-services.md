# 06 — Typed platform services and native permission enforcement

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Status and outcome

This is a **documentation-only delivery plan**. It specifies a shared application API with independently compiled QNX and Marathon backends. No service binding, permission prompt, device test, or package upgrade is implemented by this PR. Implementation owners and independent reviewers remain unassigned.

Start with a read-only network-state monitor, then add separately reviewed service families. React applications should receive typed state, explicit failures, and owned subscriptions. Fabric, Hermes, Yoga, Qt Quick, and the existing Qt completion route remain the rendering/runtime foundation.

## Evidence and lessons

The inspected RN QNX base is [`e84b51430c239bc89dbd48ac35afab89f7c77c01`](https://github.com/mikevocalz/react-native-qnx/tree/e84b51430c239bc89dbd48ac35afab89f7c77c01). Its `package.json` pins RN 0.88.1 and Nitro/Nitrogen/core `1.0.0-alpha.nightly.105.g69d365fc9b18`. Its local `scripts/nitro-platform-codegen.mjs` accepts only the small PlatformInfo proof; it rejects Promise, callback, and additional-method shapes. It is not a general upstream Nitrogen backend. These service APIs therefore require an explicit codegen extension or a clearly isolated experimental binding before runtime support can be claimed.

[#44](https://github.com/mikevocalz/react-native-qnx/pull/44), inspected at `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, already records physical Pi 400 async Nitro evidence. Its [`QT_DISPATCHER.md`](https://github.com/mikevocalz/react-native-qnx/blob/e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d/docs/nitro/QT_DISPATCHER.md) says JavaScript shares the Qt owner thread and worker completions enter through the owned queue and RN RuntimeScheduler. Reuse that path; service I/O must stay off the owner thread. Queue draining is not producer cancellation.

Marathon was inspected at [`c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5`](https://github.com/patrickjquinn/Marathon-Shell/tree/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5). Its [`ipc.rs`](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/crates/marathon-shell-native/src/ipc.rs) resolves bus callers through PID-based identity and admission checks, serves I/O asynchronously, and explicitly queues GUI operations. Its [`network.rs` client](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/crates/marathon-app-native/src/network.rs) mirrors a permissioned Network service. That separation is useful for RN QNX; copying shell globals into a React component is not.

The [`Power` client](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/crates/marathon-app-native/src/power.rs) is explicitly for system apps and uses the `system` permission. It is not evidence that every third-party RN application may read or mutate power settings. The [`security model`](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/SECURITY_MODEL.md) also distinguishes trusted, unconfined native packages from managed apps. Plan 13 must establish the managed RN identity/permission contract.

## Proposed architecture and API

Put application-facing APIs behind small Nitro resource objects. Keep service-specific native adapters outside `rnqnx_fabric_adapter`. The Marathon adapter uses reviewed shell D-Bus interfaces and actual caller identity. The QNX adapter uses an existing authorized provider where possible; Plan 07 supplies a bounded transport only when a separate service is justified. Both return the same domain types and differ in reported availability.

The following is a **proposed TypeScript contract**, not generated Nitro source. Its final v1 declaration syntax must follow the audited generator/runtime. Split public types into focused files and document every exported member before implementation.

```ts
type NetworkSnapshot =
  | { readonly state: 'connected'; readonly transport: 'wifi' | 'ethernet' | 'other' }
  | { readonly state: 'disconnected' }
  | { readonly state: 'unknown' };

interface ListenerSubscription {
  remove(): void;
}
interface NetworkMonitor {
  getSnapshot(): Promise<NetworkSnapshot>;
  addOnSnapshotChangedListener(listener: (snapshot: NetworkSnapshot) => void): ListenerSubscription;
  addOnErrorListener(listener: (error: Error) => void): ListenerSubscription;
  dispose(): Promise<void>;
}
interface NetworkService {
  getAvailability(): Promise<'available' | 'unavailable'>;
  createMonitor(): Promise<NetworkMonitor>;
}
```

`connected` means a provider reports a network link; it does not assert Internet reachability. `unknown` is real domain uncertainty, while permission refusal, missing service, malformed replies, and disposal are `Error` failures. `createMonitor()` resolves with a ready resource or rejects. It does not silently request permission. A user-initiated permission workflow must map the stable capability to the target's actual grant; QNX must not emulate a grant merely because a JavaScript flag was set.

Subscription registration is local owner-thread work. The backend caches the latest validated state and delivers an initial snapshot asynchronously through the same owner-ordered path as later updates, so callers do not need a race-prone “read, then subscribe” sequence. The initial task reads the latest state when it executes; generation/sequence checks suppress older queued observations. A provider update between registration and delivery must not be followed by an older initial value. Removal is idempotent and stops future selection of callbacks; a callback already selected for delivery may finish. Disposal closes admission, removes native subscriptions, stops producers, and completes before host teardown.

**One-shot caller:**

```ts
const monitor = await Network.createMonitor();
try {
  renderNetwork(await monitor.getSnapshot());
} finally {
  await monitor.dispose();
}
```

**Observed state with deterministic cleanup:**

```ts
const monitor = await Network.createMonitor();
const changes = monitor.addOnSnapshotChangedListener(renderNetwork);
const failures = monitor.addOnErrorListener(showNetworkError);
// Invoke this cleanup when the owning screen/session is disposed.
async function closeNetworkPanel() {
  changes.remove();
  failures.remove();
  await monitor.dispose();
}
```

**Unavailable or denied service:**

```ts
try {
  const monitor = await Network.createMonitor();
  await monitor.dispose();
} catch (error) {
  showNetworkError(error instanceof Error ? error : new Error(String(error)));
}
```

The sketches use injected UI callbacks and a proposed `Network` root. They are review examples, not runnable package exports. Do not add a JavaScript facade that changes the eventual native contract's errors or lifecycle.

## Work slices and dependencies

1. **Contract:** enumerate supported observations, grant mappings, and error semantics; add consumer fixtures with explicit unsupported/denied cases. No dependency updates.
2. **Native adapter:** implement one owner per session, typed conversion, additive listeners, and deterministic disposal. Introduce a proposed `RNQNX_ENABLE_PLATFORM_SERVICES` switch, default OFF, with unsupported-host rejection.
3. **QNX provider:** choose and document a real target provider. Prove an unavailable result when that device lacks one; a synthetic service remains test evidence only.
4. **Marathon provider:** bind reviewed interfaces under the correct app identity. Demonstrate denied and granted sessions in the managed launch lane from Plan 13.
5. **Additional domains:** review notifications, sensors/location, audio, haptics, storage, and system-only power separately. Each needs its own capability, permission, lifetime, and device-support record; no broad “all native services supported” milestone.

Plan 01/#44 provide runtime ownership; Plan 07 supplies QNX IPC where selected; Plan 05 defines visibility/background behavior; Plan 08 adds recovery. Plan 12 consumes the first real service. These are implementation dependencies, not reasons to combine independent documentation diffs.

## Proposed roster and file ownership

| Responsibility          | Proposed ownership                                                               | Independent review               |
| ----------------------- | -------------------------------------------------------------------------------- | -------------------------------- |
| API architect, DRI      | New domain specs beneath `packages/qnx-nitro-runtime/specs/` and caller examples | Nitro/JSI engineer               |
| C++ service engineer    | New focused provider/session files beneath `packages/qnx-nitro-runtime/cpp/`     | Qt ownership engineer            |
| QNX engineer            | Proposed `native/platform/qnx/services/`                                         | Security engineer                |
| Marathon engineer       | Proposed `native/platform/linux-host/services/`                                  | Shell integration reviewer       |
| QA/performance engineer | Proposed service fixtures, native tests, device receipts                         | Independent QNX regression owner |

All roles require named assignment in implementation PRs. External maintainers are reference/review targets, not claimed participants.

## Skills and applied decisions

The following exact sources were read for this plan; that is source-review evidence, not CLI installation or native-test evidence.

| Skill                                   | Source revision                                                                           | Decision applied                                                            |
| --------------------------------------- | ----------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| Margelo `api-design`                    | `1e9e17be6f41c838db472d6a4b943fc26355a3be`, `skills/api-design/SKILL.md`                  | Ready factories, real Error failures, owned listeners, explicit absence     |
| Margelo `build-nitro-modules`           | Same revision, `skills/build-nitro-modules/SKILL.md`                                      | Stateful domain objects; block unsupported general codegen claims           |
| Margelo `cpp`                           | Same revision, `skills/cpp/SKILL.md`                                                      | RAII sessions, narrow adapter files, one owner and no callbacks under locks |
| Callstack `react-native-best-practices` | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`, `skills/react-native-best-practices/SKILL.md` | Measure subscription cost and owner-thread work before optimization         |

Sources: [Margelo skills](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills), [Callstack skill](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md).

## Acceptance and validation

Require an update-between-registration-and-initial-delivery test and runtime tests for two simultaneous subscribers, independent removal, permission revocation, late completion, service loss, repeated create/dispose, and host shutdown. Denial must remain visible through the actual native service, including calls that bypass a JS convenience layer. Reject malformed native fields instead of coercing them into a plausible successful snapshot.

The first load test proposes 1,000 provider updates with a bounded latest-state slot per session; report coalescing and delivered order. Do not drop command completions or discrete events under that policy. Trace I/O and native conversion off the Qt/JS owner, publish event latency and allocation counts, and compare frame and idle metrics from Plan 03. Numerical latency budgets require a measured device baseline before approval.

Existing command names verified in base `package.json`: `pnpm validate:dependencies`, `pnpm typecheck`, `pnpm test:js`, `pnpm nitro:platform:check`, `pnpm validate:qnx-nitro`, and `pnpm test:cpp:host`. New behavioral tests must be registered in their real runners. Full Qt CTest and licensed QNX builds follow #44's recorded configuration. A Linux test cannot substitute for QNX; a host fake cannot substitute for a Marathon permission-denied device run. **None of those implementation gates was executed by this documentation PR.**

QNX resource-manager implementations must follow [permission and message validation guidance](https://qnx.com/developers/docs/8.0/com.qnx.doc.security.system/topic/manual/write_resource_manager.html). Keep logs free of credentials and network identifiers unless the test requires redacted examples. Review Qt/module licenses before new dependencies; this plan copies no shell or archived sample implementation.

## Rollout, rollback, next PR

Enable only in the conformance app after both target-specific evidence rows are explicit. Roll back by disabling the new switch and removing its package imports, after awaiting session disposal. Keep existing renderer, dependency pins, and QNX startup paths intact. The next implementation PR is the narrow network contract plus one real native read/error path; keep it draft until its actual device gate is recorded.

## Current QNX audio capability gate

The [QNX OS 8 audio migration page](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.migration/topic/audio.html) describes an initial release without an audio framework; [BSP migration](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.migration/topic/bsps.html) removes legacy audio references. Do not infer QNX playback/recording support from Marathon's Linux services, old QSA samples or a generic Qt API. Check the exact later component/BSP inventory and prove any available provider. Until then, audio-related resource creation/background admission must report unavailable rather than grant a nonfunctional capability. This gate does not claim that every later QNX distribution lacks audio.
