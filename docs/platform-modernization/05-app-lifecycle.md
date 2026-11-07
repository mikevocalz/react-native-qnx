# Plan 05: visibility, background work and state restoration

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Status and problem

This is a delivery specification. It changes documentation only; it does not add lifecycle APIs, freeze a process or claim device validation. Applications need to stop invisible rendering without interrupting permitted audio, calls, recording or useful asynchronous work. Focus loss alone cannot decide this: a window can remain visible without focus, and one process can own several surfaces.

The source baseline is `react-native-qnx` commit `e84b51430c239bc89dbd48ac35afab89f7c77c01`. Its handoff documents contain older proof statements. [PR #44](https://github.com/mikevocalz/react-native-qnx/pull/44), inspected at `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, has physical Pi 400 evidence for the Qt/Hermes/Fabric runtime and the owned Nitro completion queue. Its [dispatcher contract](https://github.com/mikevocalz/react-native-qnx/blob/e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d/docs/nitro/QT_DISPATCHER.md) says JavaScript shares the constructing Qt thread, and close drains accepted work rather than cancelling it. This plan extends those ownership rules; it does not replace the dispatcher.

## Source findings and architecture boundary

Marathon sources are pinned to `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5`:

- [Lifecycle implementation](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/crates/marathon-shell-native/src/tasks/lifecycle.rs) distinguishes Foreground, BackgroundActive, BackgroundIdle and Frozen, plus registration/termination states. It cancels stale freeze turns, checks that a first window has mapped, and retains background capability claims.
- [Hidden-frame throttle](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/shell/src/wayland/hiddenframethrottle.h) limits callbacks for unseen surfaces, while allowing visible surfaces immediately.
- [Native lifecycle contract](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/NATIVE_APP_LIFECYCLE.md) explains Linux cgroup ownership, thaw-before-close and asynchronous shell calls. Its documented hidden-frame cadence is a compositor policy, not an RN timer guarantee.

Keep Hermes, Fabric, Yoga and Qt Quick shared. Implement one native lifecycle coordinator with separate QNX and Marathon event adapters. Keep Linux cgroups, systemd scopes and D-Bus names out of shared runtime code. On QNX, begin with cooperative producer suspension and renderer idling; any stronger OS suspension requires its own reviewed backend and proof. Do not transplant Linux priorities or freezer commands.

## Proposed behavior and public contract

Track activity, per-surface visibility, focus, producer state and runtime generation separately. Derive process activity from all live surfaces and permissioned background leases. A background-active app can continue approved nonvisual work while its hidden animations remain paused. Do not automatically pause every timer: document which framework-owned producers stop, and make application work opt into an explicit background contract.

The following is a proposed API sketch, not an installed package or a verified Nitrogen spec. Split exported types into focused files during implementation and document every property, error and ownership rule.

```ts
type Activity = 'foreground' | 'background-active' | 'background-idle';
type BackgroundKind = 'audio-playback' | 'recording' | 'download';
interface LifecycleSnapshot {
  readonly activity: Activity;
  readonly hasVisibleSurface: boolean;
  readonly hasFocusedSurface: boolean;
}
interface ListenerSubscription { remove(): void }
interface BackgroundTask {
  finish(): Promise<void>;
}
interface AppLifecycle {
  readonly current: LifecycleSnapshot;
  addOnStateChangedListener(
    listener: (state: LifecycleSnapshot) => void,
  ): ListenerSubscription;
  beginBackgroundTask(options: {
    kind: BackgroundKind;
    reason: string;
  }): Promise<BackgroundTask>;
}
```

`current` is an owner-thread cached value. Beginning a task checks native authorization and resolves only after admission; unsupported or denied work rejects with a real `Error`. `finish()` is idempotent, releases native ownership, and does not claim to finish the underlying download or recorder. Native producer objects retain their own stop/dispose contracts. Add other task kinds only when a real provider and tests exist.

Two intended call sites illustrate independent ownership:

```ts
const subscription = lifecycle.addOnStateChangedListener((state) => {
  setVisualActivity(state.hasVisibleSurface);
});
setVisualActivity(lifecycle.current.hasVisibleSurface);
// Component unmount:
subscription.remove();
```

```ts
try {
  const task = await lifecycle.beginBackgroundTask({
    kind: 'download', reason: 'Save the document for offline editing',
  });
  try {
    await documentDownload.run();
  } finally {
    await task.finish();
  }
} catch (error) {
  showDownloadFailure(error);
}
```

Subscription setup and its initial snapshot read occur on the same owner thread. Generation-checked delivery prevents obsolete snapshots after reconnection. Removing a subscription stops future emissions; it need not retract a callback already selected for delivery. A frozen process cannot execute JavaScript: expose a pre-suspend opportunity and a post-resume snapshot, never a promise of JS execution while frozen.

## Implementation slices and dependencies

1. **Observe only.** Add native lifecycle/surface snapshots and transition logging behind proposed `RNQNX_ENABLE_APP_LIFECYCLE=OFF`. Inject focus/visibility events in host tests; do not suspend work yet. Reject unsupported build configurations.
2. **Idle visual producers.** Gate platform-owned frame requests, spinners and animation producers using actual surface visibility. Preserve first-map progress and modal/multiwindow visibility. Plan 03 owns presentation tracing; Plan 02 owns gesture cancellation before hiding a surface.
3. **Background admission.** Integrate Plan 06 capability checks and platform adapters. On Marathon, negotiate the runner contract in Plan 13 rather than granting permission from JavaScript. On QNX, the native policy provider owns admission.
4. **Checkpoint and resume.** Persist application state asynchronously before a bounded suspension opportunity; cancellation or timeout is explicit. Restore route, document revision, scroll anchor and safe focus after the new surface is ready. Do not persist JSI pointers or process-local surface IDs.
5. **Teardown and recovery.** Stop accepting producer work, request each producer's bounded shutdown, settle or reject its outstanding operations, then use PR #44's owner queue teardown. Plan 08 owns failed-service recovery. Queue draining alone does not establish a shutdown deadline.

Implementation prerequisites are Plan 01's runtime baseline and ownership contract, PR #44, Plan 06 for permissioned work, and Plan 13 for managed Marathon lifecycle. Plans 04 and 11 supply focus/inset/topology correctness. Plan 12 consumes the resulting behavior. These are implementation dependencies; this document can be reviewed independently.

## Roster and path ownership

All human assignments remain **unassigned**. An implementing PR must name a DRI and a different reviewer before promotion from draft.

| Responsibility | Proposed owner role and paths | Independent review |
| --- | --- | --- |
| Coordinator and runtime generation | Qt/JSI engineer; `vnext/src/runtime`, corresponding public headers | Hermes/runtime engineer |
| Public lifecycle API | Native API architect; focused files in the existing native-module package selected at implementation | Nitro/codegen engineer |
| QNX backend | QNX systems engineer; `native/platform/qnx` | QNX regression owner |
| Marathon backend | Wayland integration engineer; `native/platform/linux-host` | Shell lifecycle/security maintainer |
| Failure and retention evidence | QA engineer; `tests/cpp`, `tests/js`, Plan 12 application | Performance reviewer |

## Skills and decisions

The actual source files were read for this design. This records source-guided planning, not CLI installation, native execution or vendor approval.

| Exact skill | Pinned source | Decision applied |
| --- | --- | --- |
| `api-design` | [Margelo, `1e9e17be6f41c838db472d6a4b943fc26355a3be`](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/api-design/SKILL.md) | Cached observations, async admission, explicit additive subscriptions and error paths |
| `build-nitro-modules` | [Same revision](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/build-nitro-modules/SKILL.md) | Native task ownership; typed specs before bindings; validate generator support for these async/resource shapes |
| `cpp` | [Same revision](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/cpp/SKILL.md) | One owner queue, RAII and generation-scoped resources; no sleeps as readiness fixes |
| `react-native-best-practices` | [Callstack, `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md) | Compare identical foreground/background workloads before tuning |

The repository's limited platform-info code generator does not prove generation of this richer lifecycle API. Compile and deterministic-codegen evidence remain required.

## Acceptance, performance and evidence

| Scenario | Required observable result |
| --- | --- |
| Focus lost but window still visible | No premature suspension; input focus updates once |
| Last surface hidden; no lease | Visual producers settle, then issue no application-driven frames in the quiescent fixture |
| Hidden audio/download lease | Authorized producer continues; hidden visual work remains idle; releasing the final lease changes activity |
| Hide then immediately show | Old freeze/checkpoint replies cannot suspend the new generation |
| First window still loading | App reaches its first frame without being frozen mid-launch |
| Service disconnect or permission revocation | Lease failure is visible, work stops according to its contract, and no permission is silently retained |
| 100 hide/show and close/reopen cycles | State restores, subscription/native resource counts return to baseline, and no callback enters destroyed Hermes |

Record idle CPU, wakeups, frame requests, resume-to-first-presentation p50/p95/p99 and retained memory. Set device-specific absolute budgets from Plan 03's repeated baseline before implementation promotion; this plan reports no measured improvement. A hidden compositor callback is not proof that RN actually repainted.

Existing command recipes, **not executed by this documentation change**, include `pnpm validate:dependencies`, `pnpm typecheck`, `pnpm test:js`, `cmake --preset linux-host-renderer`, `cmake --build --preset linux-host-renderer`, and `ctest --preset linux-host-renderer`. New transition tests must appear in CTest's executed inventory. Use PR #44's pinned QNX runbook for physical evidence and `docs/marathon/NATIVE_HOST.md` for the separate Linux host lane. Run actual Marathon suspend/resume on its compositor; offscreen rendering cannot pass that gate.

## Security, licensing and rollback

Background claims require native identity/permission checks and cleanup on process death. Persist only required application state; redact document contents and secrets from lifecycle logs. Review licenses before reusing Marathon source; this proposal borrows behavior and contains no copied implementation. Keep QNX and Marathon resource policies independent.

Disable the new flag and return to existing producer behavior to roll back. Make checkpoint formats versioned and retain a compatible reader before writing new state. Do not alter boot configuration in this workstream. The next implementation PR should deliver the observe-only snapshot with cancellation/generation tests and exact source/build evidence, still in draft until the relevant target runs.

## Current QNX audio capability gate

The [QNX OS 8 audio migration page](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.migration/topic/audio.html) describes an initial release without an audio framework; [BSP migration](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.migration/topic/bsps.html) removes legacy audio references. Do not infer QNX playback/recording support from Marathon's Linux services, old QSA samples or a generic Qt API. Check the exact later component/BSP inventory and prove any available provider. Until then, audio-related resource creation/background admission must report unavailable rather than grant a nonfunctional capability. This gate does not claim that every later QNX distribution lacks audio.
