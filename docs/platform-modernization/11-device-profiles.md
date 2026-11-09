# Plan 11: device profiles and adaptive window metrics

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Implementation update — November 9, 2026

The [adaptive-window implementation receipt](../evidence/adaptive-window-layouts-2026-11-09.md)
records the first distinct-window-metrics slice, shared adaptive gallery/starter,
native typography and state-retention checks. This slice repairs the existing RN
`DeviceInfo` compatibility path and preserves the standard public `Dimensions`
and `PixelRatio` contracts. It does not introduce the proposed application-facing
profile module or a replacement for the pinned Nitro v1 policy.

The broader profile schema, input/keyboard-inset provider, segmentation,
physical cross-display moves and device matrix below remain future work.
`fontScale` has a validated host integration seam with baseline 1.0; it is not yet
connected to an operating-system accessibility preference. The proposed
`RNQNX_ENABLE_DEVICE_PROFILES` flag below applies to that future provider work;
this shared compatibility repair adds no new CMake flag. Target qualification is
separate from the Linux host evidence in the receipt.

## Status and concrete gap

This PR contains a plan, not a new device backend or hardware-support claim. Implement a portable description of the current window, display and input capabilities so the same React Native application can behave correctly on QNX, Marathon phones, keyboard handhelds, multiple displays and future foldable hardware.

The inspected RN QNX base is `e84b51430c239bc89dbd48ac35afab89f7c77c01`. In [QnxDeviceInfo::getConstants](https://github.com/mikevocalz/react-native-qnx/blob/e84b51430c239bc89dbd48ac35afab89f7c77c01/vnext/src/runtime/QnxExceptionsManager.cpp), both `windowPhysicalPixels` and `screenPhysicalPixels` receive the primary screen's metrics. The source explicitly lacks separate window metrics. [Component support](https://github.com/mikevocalz/react-native-qnx/blob/e84b51430c239bc89dbd48ac35afab89f7c77c01/docs/component-support.md) also records zero safe-area insets and an unwired software keyboard. A resized window, keyboard occlusion or different display scale therefore needs more than a new board-name constant.

Existing [Marathon host evidence](https://github.com/mikevocalz/react-native-qnx/blob/e84b51430c239bc89dbd48ac35afab89f7c77c01/docs/marathon/NATIVE_HOST.md) proves a Linux offscreen build/run at its recorded CI revision. [PR #44](https://github.com/mikevocalz/react-native-qnx/pull/44), inspected at `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, separately records the physical Pi 400 runtime. Neither result establishes adaptive behavior on a phone or folding display.

## Marathon lesson and portable boundary

The pinned [device overlay guide](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/DEVICE_OVERLAYS.md) at `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5` separates runtime probes, generated device profiles and image overlays. Its checks connect manifest fields to consumed code, validate GPU capabilities and keep hardware-specific choices outside the shell core. That separation is the useful pattern.

Adopt the separation in RN QNX without importing Marathon's Linux `/sys`, DRM, firmware or governor implementation into QNX. Native platform adapters gather observations; a shared snapshot model conveys those observations to the renderer and application. Board profiles supply only facts unavailable through reliable probes. Fabric and Yoga remain responsible for mounting and layout; profiles never set Qt anchors or create a second layout system.

Treat display identity, operating system and JavaScript platform identity as different concepts. A Marathon profile cannot turn a QNX binary into a Linux binary. A software-rendered host preview cannot establish a physical GPU capability.

## Proposed data contract

Preserve RN's existing `Dimensions` and `PixelRatio` contracts. Before changing them, inspect the exact RN 0.88.1 conversion path and prove logical-to-physical unit conversion end to end; avoid both missing and duplicate scaling. An optional native module supplies richer window segmentation and input facts. These types are proposed design sketches, not generated or installed bindings:

```ts
interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}
interface WindowMetrics {
  readonly bounds: Rect; // Window-local logical units.
  readonly scale: number; // Device pixels per logical unit.
  readonly refreshRateHz?: number; // Absent when unobserved.
  readonly safeInsets: { top: number; right: number; bottom: number; left: number };
  readonly segments: readonly Rect[];
  readonly topology: 'single-region' | 'segmented' | 'unknown';
  readonly inputModes: readonly ('touch' | 'pointer' | 'keyboard' | 'pen')[];
}
interface WindowProfile {
  readonly current: WindowMetrics;
  addOnChangedListener(listener: (metrics: WindowMetrics) => void): {
    remove(): void;
  };
}
```

`current` is a cheap owner-thread snapshot. Coordinates have a documented origin and units. `segments` describe usable regions of this window, not a union of unrelated monitors. `unknown` topology must not be reported as a confirmed absence of a hinge. Keep diagnostics with provider/provenance information separate from the public application model; do not expose serial numbers or device fingerprints by default.

Two intended uses make missing information and cleanup explicit:

```ts
const subscription = profile.addOnChangedListener(updateLayoutConstraints);
updateLayoutConstraints(profile.current);
// When the surface owner unmounts:
subscription.remove();
```

```ts
const metrics = profile.current;
if (metrics.topology === 'segmented' && metrics.segments.length === 2) {
  arrangeEditorAndPreview(metrics.segments);
} else {
  arrangeResponsiveEditor(metrics.bounds);
}
```

The second branch is an ordinary responsive layout, not simulated proof of foldable support. Keyboard insets belong to Plan 04's live input-method provider and are not permanently baked into the board profile. Input devices can appear or disappear; pen support requires an actual provider and event contract, not a model-name guess.

## Resolution and update rules

Keep static boot traits separate from dynamic surface metrics. A board profile may request an initial graphics capability ceiling; actual negotiated backend capabilities determine the resolved result. Live window size, display membership, scale, occlusion and input-device events supersede startup guesses. Explicit test overrides are allowed only in a diagnostic mode whose output is marked simulated.

Validate finite positive scale, coordinate bounds, segment containment, inset geometry and capability enums at the boundary. Reject malformed required profile data with an actionable diagnostic. Missing optional physical dimensions or refresh observations remain unknown. Do not silently manufacture 60 Hz, a hinge, a touchscreen or a physical keyboard. Publish a coherent snapshot per change, coalescing redundant updates without dropping a terminal resize/orientation state.

Moving between displays must update the surface's scale and dimensions from that surface's actual screen. Display removal invalidates its observations and resolves the remaining surface geometry. Stable identity within a running process is sufficient; persistent cross-device identifiers require a separate privacy/use-case decision.

## Implementation slices and dependencies

1. **Distinct window metrics.** Refactor the existing `QnxDeviceInfo` seam into focused implementation files, retaining compatibility. Connect the host surface's Qt geometry and screen-change signals. Add no board-specific branch.
2. **Profile validation and diagnostics.** Introduce versioned profiles under proposed `config/device-profiles/`, a schema and deterministic validation. Record the provider for each overridden fact. Keep default configuration behavior unchanged.
3. **Adaptive input and insets.** Connect Plan 04's keyboard/focus and Plan 05's surface visibility. Update RN dimensions and safe-area consumers through their audited contracts, with exactly one layout authority.
4. **Multiple displays and segmentation.** Implement actual provider interfaces and injected host fixtures. Enable a hardware provider only after its API/source audit and target evidence. Label foldable fixture runs simulated until physical hardware passes.
5. **Consumer proof.** Use Plan 12's editor/list application at several sizes and inputs; preserve document, selection and scroll state during topology changes.

Use proposed `RNQNX_ENABLE_DEVICE_PROFILES=OFF` until the device matrix passes. Reject providers for unsupported OS/backend combinations. Plan 01 supplies the runtime baseline; Plans 03, 04 and 05 supply measurement, text and lifecycle contracts; Plan 13 supplies Marathon window/shell integration. These are implementation dependencies, not a requirement to merge sibling plan documents first.

## Roster and skills

Human DRIs and independent reviewers are **unassigned**; fill them in when implementing.

| Role                      | Owned paths/deliverable                                                                  | Independent reviewer               |
| ------------------------- | ---------------------------------------------------------------------------------------- | ---------------------------------- |
| Qt/Fabric engineer        | `vnext/src/runtime/QnxExceptionsManager.cpp`, focused metrics files, surface constraints | RN runtime engineer                |
| QNX systems engineer      | `native/platform/qnx`, board-profile sources                                             | QNX regression owner               |
| Marathon/Wayland engineer | `native/platform/linux-host`, output/scale observations                                  | Shell integration reviewer         |
| API designer              | Proposed `packages/platform-window-metrics`, documented units and availability           | Nitro/codegen engineer             |
| Device QA                 | `tests/cpp`, `tests/js`, Plan 12 fixture scenarios                                       | Accessibility/performance reviewer |

The ordered Margelo sources `api-design`, `build-nitro-modules`, and `cpp` were read at [`1e9e17be6f41c838db472d6a4b943fc26355a3be`](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills). They led to cached immutable observations, explicit absence, additive subscriptions, focused files and owner-thread lifetime. [Callstack `react-native-best-practices`](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md), read at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`, requires identical workloads before/after tuning. These are source-guided design decisions, not installed CLI or runtime test claims. Verify generation of arrays, optional observations and subscription shapes; the existing limited platform-info generator is not proof for this module.

## Acceptance and validation

| Fixture or device case                   | Required result                                                                                 |
| ---------------------------------------- | ----------------------------------------------------------------------------------------------- |
| Resize window inside unchanged screen    | Window dimensions change; full-screen dimensions retain the correct display value               |
| Scale 1, 1.25 and 2 fixtures             | Rendered geometry and hit testing agree; no double scaling                                      |
| Window moved between displays            | Metrics follow the actual destination screen; focus and input targets remain correct            |
| Keyboard appears/disappears              | Insets update without overwriting physical display facts or losing selection                    |
| Two-region fold fixture                  | Controls avoid the occluded region; editor state survives transition; evidence marked simulated |
| No topology/input provider               | Unknown facts remain unknown; ordinary responsive UI works without fabricated capabilities      |
| Malformed profile or unplug during event | Bounded diagnostic/fallback behavior, no crash, and no stale display callback                   |
| 100 topology/resize cycles               | Final snapshot correct, subscription counts stable and no monotonic native-memory growth        |

Run the existing `pnpm validate:dependencies`, `pnpm typecheck`, `pnpm test:js` and full `linux-host-renderer` CMake/CTest preset after implementation. Register new native cases in executed CTest inventory; fixture-only JavaScript tests cannot prove Qt signal wiring. These commands were inspected, not run for this documentation change. Exact schema validator and device scenario commands must be checked in with their implementation, not invented here.

Capture a QNX physical run separately from a Marathon Wayland run, recording window/display pixels, logical sizes, scale, actual input devices, GPU/Qt build and commit hashes. Phone and keyboard-handheld evidence are separate rows. Record resize-to-presentation p50/p95/p99 and layout count against Plan 03's measured budget; no percentage improvement is claimed. Profiling must show bounded work per snapshot and no periodic polling while idle.

## Security, licensing, next PR and rollback

Profiles are bounded data, never executable shell fragments or arbitrary paths loaded from the app. Do not disclose hardware identifiers in telemetry without a stated need. Retain Qt and device-driver license/provenance records; reference Marathon's approach without copying its image overlays into the QNX distribution.

The first implementation PR should fix and test distinct window/screen metrics behind the flag. Roll back by disabling the feature and restoring the previous metrics adapter; profile format readers must remain compatible with any persisted configuration. This work does not change firmware, partitioning or boot policy, and it cannot promote an untested board to supported status.
