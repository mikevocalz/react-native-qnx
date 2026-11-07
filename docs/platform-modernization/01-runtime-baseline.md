# Plan 01 — Reproducible runtime, startup recovery and thread ownership

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

**Status: delivery plan; no runtime or boot configuration changes in this PR.**
The goal is a repeatable, recoverable React Native process on the physical QNX target, followed by an evidence-based threading decision. This plan extends the existing Pi runtime work. Its acceptance criteria are future gates, not results of this documentation change.

## Source baseline and existing proof

The reviewed main branch is `e84b51430c239bc89dbd48ac35afab89f7c77c01`. Its package manifest pins RN `0.88.1`, React `19.3.0`, Expo `58.0.22`, Metro `0.87.1`, and Nitro core/adapter/Nitrogen `1.0.0-alpha.nightly.105.g69d365fc9b18`. Several README and handoff paragraphs describe older dependencies and an unproven QNX runtime. Read the actual manifest and active PR evidence before using those paragraphs as current status.

[PR #44](https://github.com/mikevocalz/react-native-qnx/pull/44), inspected at `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, records physical Pi 400 execution of RN/Fabric/Hermes/Qt Quick, fullscreen capture, synchronous Nitro checks, and the owned Qt/Nitro asynchronous completion experiment. Its [dispatcher contract](https://github.com/mikevocalz/react-native-qnx/blob/e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d/docs/nitro/QT_DISPATCHER.md) reports typed/void Promise and callback behavior, microtask checkpoints, accepted-work draining and teardown races. Those are historical prerequisite results at that revision, not this plan's test results.

The [Pi runbook](https://github.com/mikevocalz/react-native-qnx/blob/e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d/docs/getting-started/pi400-hello-world.md) also records failed automatic graphics boot experiments. The verified recovery boot leaves automatic graphics startup disabled and uses controlled manual graphics setup. A rendered Hello World therefore does not yet prove reliable cold boot, frame pacing, memory budgets or production distribution rights.

## Architecture and ownership

Keep `rnqnx_react_host`, `rnqnx_fabric_adapter` and `rnqnx_views_qt` as shared source. Keep QNX's Qt QPA/Screen path and the Linux Qt Wayland client separate. Hermes executes JavaScript on the constructing Qt owner thread in #44; this plan must not describe it as an existing dedicated JS thread.

| Work                                             | Owner in the current candidate               | Required constraint                                                             |
| ------------------------------------------------ | -------------------------------------------- | ------------------------------------------------------------------------------- |
| Qt item creation/mutation and host lifecycle     | Qt owner thread                              | No worker may directly mutate a view or destroy an active runtime.              |
| JS entry and completion settlement               | Existing owned queue and RN RuntimeScheduler | Preserve microtask/render checkpoints and error forwarding.                     |
| Blocking QNX calls, Forge work, decoding and I/O | Explicit native worker/queue                 | Never block the GUI/JS owner waiting for another process or callback.           |
| Scene-graph work                                 | Qt's documented render-thread contract       | Do not infer GUI-thread safety from a signal name.                              |
| Producer shutdown                                | Each native binding's owner                  | Stop admission and quiesce producers before the host's completion queue closes. |

Queue close drains accepted work; it does not cancel a stuck worker or impose a deadline. A non-returning accepted task, or owner-thread work that continually adds transitive settlement work, can prevent the drain from completing; late worker posts are rejected after admission closes. A worker waiting synchronously for JS can deadlock if the owner joins it. Binding lifecycle tests must address both cases before a module joins this runtime.

## Proposed roster and paths

Human assignments are unassigned. The **QNX runtime engineer** is the DRI for `native/platform/qnx`, startup/run tooling and evidence manifests. The **Qt/JSI engineer** owns `vnext/src/runtime/QtReactHost.cpp` and the queue/dispatcher integration. The independent **QNX regression reviewer** checks toolchain and recovery behavior; the independent **C++ lifetime reviewer** checks closure and runtime ownership. A release reviewer checks the actual Qt module and license scope before redistributable packaging.

Changes to the shared host require coordination with plans 02, 03, 05 and 10. The implementation PR must list precise files after rebasing #44, including any queue headers introduced there; this plan does not invent a second queue implementation.

## Delivery slices

1. **Reconcile the baseline record.** Verify that the selected main baseline includes the reviewed #44 candidate and its integration changes. Update the current-state and component evidence tables with per-revision results, removing contradictory blanket claims. Preserve historical tests and record the exact dependency graph, toolchain, image, binary, plugins, bundle and deployment hashes.
2. **Repeat application launch on the known recovery setup.** Add a bounded application-level harness around the existing visual runner. Verify launch, first rendered content, Nitro operation, clean shutdown and failure exit reporting. Keep direct-Screen probes labeled as lower-layer checks. A successful probe must not stand in for RN rendering.
3. **Prove shutdown and recoverable failure.** Re-run the existing dispatcher behavior suite and add only missing producer-level cases needed by a real binding. Exercise initialization failure, a worker error, late completion, app exit under queued work and a deliberate child-process crash. A supervisor must preserve logs and terminate a bounded test that does not exit.
4. **Investigate graphics startup as a separate device change.** Capture the known-good manual startup sequence and failed-boot evidence without assuming their cause. Establish a tested recovery procedure and reachable console/network before any boot experiment. Stage one change at a time, retain the known recovery image and verify device health after each bounded trial. This documentation PR does not execute that work or authorize a different system image.
5. **Decide whether to separate JS from the GUI thread.** First use plan 03 to identify actual owner-thread blocking. Move heavy native work to its existing owner before adding another runtime thread. If measured JS work still prevents the required interaction, write a focused ADR covering EventBeat, RuntimeScheduler, Fabric mounting, JSI runtime ownership, synchronous RN paths, teardown and error delivery. Compare one controlled candidate against the current shared-thread baseline. Do not promise that another thread alone solves presentation latency.

New experimental switches must default OFF and reject unsupported configurations. Reuse #44's `RNQNX_ENABLE_NITRO_QT_DISPATCHER` and existing visual-host switches where they already express the experiment; do not add duplicate knobs. No package or Nitro ABI upgrade belongs in the startup slice.

## Validation and measurable exit criteria

| Gate                         | Required evidence                                                                                                                     | Failure condition                                                                                     |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| Candidate provenance         | Commit, lock/bundle/binary hashes, SDP/Qt/Hermes IDs, board/image, flags and plugins                                                  | A receipt cannot identify what ran.                                                                   |
| Host ownership               | Existing runtime/dispatcher CTests plus the affected producer cases under supported ASan/UBSan                                        | Wrong-thread mutation, callback after runtime destruction, double settlement or leaked accepted task. |
| Configuration                | Relevant graph builds with experiments ON and OFF; unsupported configuration fails                                                    | The OFF path or original QNX probe changes unexpectedly.                                              |
| Manual runtime repeatability | Proposed minimum: 10 launch/render/Nitro/exit trials on the known recovery setup, all bounded and separately recorded                 | A hang, missing first frame, nonzero unexplained exit, stale process or lost access.                  |
| Recovery                     | An injected app failure leaves the target recoverable and the next controlled app launch succeeds                                     | Recovery requires an undocumented boot/library replacement.                                           |
| Cold boot                    | After a separately reviewed startup fix, proposed minimum: 10 controlled boot/application cycles with HDMI and remote-health evidence | A boot loses expected display/network reachability or requires an unrecorded manual repair.           |
| Threading decision           | Comparable latency/frame and memory measurements from plan 03                                                                         | A thread change is promoted without an improvement or a justified tradeoff.                           |

Trial counts are proposed release criteria, not measured reliability guarantees. They do not establish a failure probability or real-time bound. Set explicit trial timeouts and the product's startup target after the first baseline measurement; do not invent millisecond results in this plan.

### Existing command entry points

The following repository commands were inspected, not run for this documentation PR:

```sh
pnpm install --frozen-lockfile
pnpm validate:dependencies
pnpm typecheck
pnpm test:js
pnpm nitro:platform:check
pnpm validate:qnx-nitro
```

For the full native candidate, use the prerequisite's documented configuration and actual licensed dependency paths, then:

```sh
cmake --build /absolute/full-host-build --parallel 4
ctest --test-dir /absolute/full-host-build --output-on-failure
cmake --build /absolute/qnx-build --parallel 4 --target rnqnx_nitro_qt_test rnqnx_playground_qnx
```

The absolute build directories are operator-supplied placeholders, not directories created by this plan. Use the matching host Hermes compiler for QNX bytecode and deploy its fixtures with the executable. `pnpm pi:preflight` is a prerequisite check, not evidence of a complete RN app. Execute deployment/boot commands only in their reviewed target runbook.

For Marathon, preserve the existing Linux host build and QNX-independent linking. Record Linux offscreen, Linux Wayland and actual Marathon-shell tests separately. A QNX result does not prove the Linux application lifecycle, packaging or permissions.

## Skills, limits and rollback

Margelo `api-design` → `build-nitro-modules` → `cpp` at `1e9e17be6f41c838db472d6a4b943fc26355a3be` informed the async boundary, generated-ABI audit and producer lifetime requirements. Callstack `react-native-best-practices` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` requires the measured threading decision. Source reading is recorded; these sources do not prove a test run or installed device tooling. Apply the repository native and validation skills when implementation begins.

Do not copy Linux `SCHED_RR` priorities or cgroup freezer controls from Marathon into QNX. QNX real-time scheduling does not make the JS event loop, GC or UI deadline deterministic. The performance workstream owns measured budgets. Limit changes to required Qt modules and the authorized license scope; do not redistribute the evaluation runtime or private target logs through a public Marathon RFC.

Rollback a runtime experiment by disabling its existing opt-in flag and redeploying the recorded known-good application/dependency closure. Keep the verified recovery boot configuration until a separately reviewed startup change passes its own gate. Documentation rollback is reverting this document; it has no device effects.

**Dependencies:** reviewed #44 implementation for its candidate; plan 00 for coordination. Plan 03 is a later decision gate for optional thread separation, not a prerequisite for recording or reproducing the baseline. **Next implementation PR:** baseline/evidence reconciliation and a bounded launch/exit harness on the preserved recovery setup.
