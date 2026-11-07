# Plan 03 — Frame pacing, input latency and reproducible performance evidence

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Deliverable and evidence boundary

This documentation PR specifies an opt-in tracing and measurement lane, followed by evidence-driven scheduling improvements. It changes no frame scheduler and records no new benchmark result. The outcome is a trace that explains where an interaction waits, a repeatable physical-device benchmark, and a performance budget for each supported display profile.

Main is inspected at `e84b51430c239bc89dbd48ac35afab89f7c77c01`. [PR #44](https://github.com/mikevocalz/react-native-qnx/pull/44), head `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, proves RN/Fabric/Hermes/Qt Quick rendering and the opt-in Nitro completion queue on a Pi 400. It reports no QNX FPS, startup or RSS measurement. Its host mounting microbenchmarks are useful leads, including dense sibling filtering cost, but are not frame times or QNX speedups. JavaScript currently shares the constructing Qt GUI thread; trace and model that topology truthfully.

## What to transfer from Marathon

Use the method in [Marathon's scrolling investigation](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/perf/SCROLLING.md), pinned to `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5`: correlate input, application work and displayed movement; count repeated frames; examine idle rendering; replay real panel cadence; retain failed experiments. Its own capture correction shows why a capture pipeline must be calibrated before interpreting latency.

That source explicitly records that a fixed late frame callback harmed a real client and that frame-start callbacks became the default. It also separates shell resampling from client input. Do not copy an 8 ms phase offset, Linux scheduler priority, touchscreen sampling rate or fling constant into QNX. The current device, QPA plugin, compositor and render loop determine the useful experiment.

[HiddenFrameThrottle](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/shell/src/wayland/hiddenframethrottle.h) demonstrates bounded callbacks for unseen surfaces and immediate treatment of shown surfaces. Here, Plan 05 supplies app visibility/background policy. A client optimization cannot commandeer Marathon's compositor clock or suspend an audio-capable background app indiscriminately.

## Instrumentation contract

Add a proposed `RNQNX_ENABLE_FRAME_TRACE` flag, default OFF and rejected on unsupported configurations. Keep tracing internal to the runtime; no general application-facing Nitro performance API is required for the first slice. Each event records a monotonic timestamp, clock identity, thread identity, surface identity, interaction/frame correlation ID and stage. The schema and encoding must be versioned. Record dropped trace records explicitly.

Capture these stages where the actual platform exposes them:

| Stage | Owner / meaning |
| --- | --- |
| Input receipt | Qt pointer event and original event timestamp; distinguish receipt time from hardware sample time. |
| Native dispatch | Fabric/RNGH arbitration and enqueue, including coalescing or cancellation. |
| JS/worklet execution | Start/end on the recorded RN or Worklets runtime owner, without assuming separate OS threads. |
| Fabric work | Commit, mounting transaction and Qt mutation application. |
| Qt frame work | Public render-loop callbacks and available synchronization/render timing. |
| Presentation | Backend/compositor presentation evidence when obtainable; otherwise label swap/submission timing as such. |

An event reaching `frameSwapped` is not automatically physical input-to-photon evidence. If display presentation timestamps are unavailable on a target, publish input-to-submit separately and use an independently calibrated camera/scanout method for visible latency. Do not subtract timestamps from different clock domains without a measured mapping and uncertainty.

Use a bounded recorder with preallocated storage or another measured low-overhead design. Avoid per-frame synchronous file writes, console formatting, blocking IPC and mutexes held across JS or Qt callbacks. Export traces after the interaction or through a bounded worker path. Sampling must never alter event delivery or run application code. Surface generation identifiers prevent a reused native address from joining unrelated traces.

Two required trace scenarios define how this data will be consumed:

1. **Drawer drag under load:** correlate each accepted contact sample through the real worklet/property path to a visibly moved frame, while a bounded non-premium compute fixture occupies its worker. An authorized Forge workload may provide a separate optional compatibility measurement. Report missed or coalesced input explicitly. A shorter JS task alone does not establish lower displayed latency.
2. **Return from background:** mark visibility change, first scheduled frame and first displayed content; distinguish app-owned animation requests from compositor repaint and approved background service activity. This catches both resume stalls and unnecessary continuous rendering.

## Implementation sequence and ownership

1. Define the trace schema and prove clock/ID ordering in deterministic host tests. Add markers only at existing boundaries in `vnext/src/runtime/QtReactHost.cpp`, `vnext/src/fabric/QtTouchDispatcher.cpp`, `ShadowViewMutationAdapter.cpp` and `QtQuickMountingLayer.cpp`; place recorder/serialization in focused new files.
2. Add QNX tracing integration in `native/platform/qnx/` and Linux/Wayland presentation integration in `native/platform/linux-host/`, each opt-in. Inspect the installed Qt/QNX headers and supported tracing APIs before naming an implementation function. QNX kernel tracing and application markers are complementary; importing Linux ftrace commands is not a QNX implementation.
3. Add a deterministic interaction driver and artifact manifest under proposed `tests/performance/` and `scripts/performance/`. Include release bundle/executable hashes, exact Qt/RN/Hermes/Nitro versions, panel mode/refresh, CPU/GPU policy, thermal state, build type and capture settings.
4. Measure the baseline before changing scheduling. Prioritize the measured longest wait or repeated-frame cause: blocking GUI work, redundant invalidation, dense mounting work, event batching or missed callback timing.
5. Change one cause at a time, remeasure with the same workload, and retain an OFF baseline. Consider a JS-thread split only through Plan 01; Worklets/gesture changes depend on Plan 02. Plan 11 defines device profiles and Plan 12 supplies the shared app.

Proposed roles below are responsibilities, not staffed assignments. Each implementation PR must name a DRI and a different reviewer.

| Role | Path ownership and deliverable | Blocking review |
| --- | --- | --- |
| Runtime/performance DRI | Runtime/Fabric trace points and bounded recorder | Qt render-loop engineer |
| QNX systems engineer | QNX trace adapter and exact target clocks | QNX scheduling reviewer |
| Marathon/Wayland engineer | Client presentation and visibility integration | Compositor-interface reviewer |
| Benchmark/QA engineer | Driver, manifests, raw samples, capture calibration | Independent statistics/evidence reviewer |
| Release engineer | Opt-in build/configuration and per-profile budgets | QNX regression owner |

## Skill evidence

The following pinned sources were read and applied to this plan. They do not establish tool installation, benchmark execution or platform support.

| Skill | Source revision | Decision |
| --- | --- | --- |
| [api-design](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/api-design/SKILL.md) | Margelo `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Keep the first tracing contract internal; expose units and meaning rather than an ambiguous FPS property. |
| [build-nitro-modules](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/build-nitro-modules/SKILL.md) | Same Margelo revision | Avoid extra queue hops and synchronous native work in interactive callbacks; add no unnecessary HybridObject. |
| [cpp](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/cpp/SKILL.md) | Same Margelo revision | Give buffers and thread-affine state explicit owners; separate capture from serialization. |
| [react-native-best-practices](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md) | Callstack `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Measure, optimize, remeasure and revert changes that do not improve the actual interaction. |

Platform manuals and Marathon code are source audits, not invented “QNX performance” skills. Read Callstack's CI guidance at a recorded revision if a later slice changes workflows. Device automation examples for iOS/Android are not evidence that those tools support QNX.

## Acceptance, budgets and commands

Before optimization, commit a per-profile budget record with refresh period, workload, baseline distribution, proposed target, allowed regression and method. For example, a verified 60 Hz mode has a roughly 16.67 ms refresh period, but not every pipeline stage gets that whole interval. Allocate stage budgets from the measured topology; do not advertise a universal frame guarantee.

Required benchmark output includes p50/p95/p99 and maximum input-to-visible latency where measurable, application frame intervals, repeated displayed frames, missed presentation deadlines, idle frame requests, cold-start first visible content, warm resume and process memory. Keep startup from process creation separate from SCP/deployment time. Report sample counts, warmups, failed runs and thermal drift.

Proposed reproducibility gates are 30 cold launches, 100 repetitions of each named interaction and a 60-second settled-idle capture per device profile. Compare tracing ON/OFF; target no more than 2% median runtime overhead before using it for optimization decisions, with raw variation reported. These are acceptance targets, not achieved measurements. A trace overflow or broken clock mapping invalidates that capture; it must not silently produce favorable percentiles. A non-animating settled app should request no continuous app-owned redraws, with documented exceptions for approved live content.

Implementation commands available in the inspected repository include:

```bash
pnpm validate:dependencies
pnpm typecheck
pnpm test:js
cmake --preset linux-host-renderer
cmake --build --preset linux-host-renderer
ctest --preset linux-host-renderer
```

These are future commands, not executed results from this plan. Configure new tracing options and real target adapters explicitly once they exist. Host sanitizer/correctness coverage is separate from release performance runs. QNX must use the licensed cross-build and physical device; Marathon must use its actual compositor and client confinement. Offscreen host rendering cannot measure either display pipeline. Exact benchmark commands remain pending the driver implementation rather than being fictional commands in this document.

## Security, rollout and rollback

Trace identifiers and durations, not typed text, passwords, document payloads or credentials. Bound recording duration/size and protect exported artifacts. Require separate review before enabling privileged kernel tracing; the app itself must not gain root or broad abilities for profiling. This plan vendors no upstream source or binaries and changes no Qt/QNX licensing basis.

Roll out the recorder OFF, enable in a named benchmark build, approve budgets, and only then propose a targeted optimization. Keep frame pacing changes distinct from priority/affinity experiments; no claim that RN JavaScript becomes hard real-time by running on Neutrino. Rollback removes this document, or later disables the trace/optimization flag and restores the measured baseline. The next slice is schema plus bounded markers and clock validation, not speculative scheduler tuning.
