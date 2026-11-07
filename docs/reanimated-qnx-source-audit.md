# Reanimated / Worklets QNX source audit

Audited 2026-11-07. This records port prerequisites; it does not enable either
package. Native touch cancellation and coordinate fixes improve the existing
Fabric input path, but do not implement RNGH recognition or Reanimated worklets.
No package, lockfile, renderer, or dependency-version change is required by this
audit.

## Verified upstream versions

The release tags below were resolved through the upstream GitHub commits API.
All paths are in `software-mansion/react-native-reanimated`.

| Package    | Release  | Exact commit                               | Compatibility evidence                                                                                                                                                                                                    |
| ---------- | -------- | ------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Reanimated | `4.7.1`  | `5fd93c32624c64c4020fb089171afde88a511d92` | [compatibility.json](https://github.com/software-mansion/react-native-reanimated/blob/5fd93c32624c64c4020fb089171afde88a511d92/packages/react-native-reanimated/compatibility.json): Fabric RN 0.86–0.88; Worklets 0.13.x |
| Worklets   | `0.13.1` | `24e415d2c036a532cf7820ff0c400e49841bda50` | Stable release corresponding to the required 0.13.x family; native interfaces inspected below                                                                                                                             |

RN 0.88.1 falls within that compatibility family. This is not proof of Expo 58
integration, QNX support, matching QNX binaries, or device execution. Reanimated
4.6.x / Worklets 0.12.x compatibility stops at RN 0.87. Main was
`d0d4dc099da98f4aa0fd0488df603e6c5b5b19fc`, advertising `4.8.0-main` and
`0.14.0-main`; those prereleases are not selected dependencies.

## Confirmed compile blocker

At Reanimated 4.7.1,
[`Common/cpp/reanimated/Tools/PlatformDepMethodsHolder.h`](https://github.com/software-mansion/react-native-reanimated/blob/5fd93c32624c64c4020fb089171afde88a511d92/packages/react-native-reanimated/Common/cpp/reanimated/Tools/PlatformDepMethodsHolder.h)
(blob `08c3c9b893b2622ac056cae4c2d85476b5356c72`) declares
`SynchronouslyUpdateUIPropsFunction` only under `ANDROID` or `__APPLE__`, then
uses it unconditionally in `PlatformDepMethodsHolder`. QNX needs an explicit
platform/generalization patch before this header compiles. Defining another
platform's macro would conceal incompatible implementation assumptions.
This is a source finding, not an exhaustive compilation audit.

## Ownership and next port seams

Keep Hermes, Yoga, Fabric and the existing Qt Quick component backend. The
integration belongs in `react-native-qnx` runtime/platform adapters; NYC-Tron UI
(Neon UI) can consume the established React APIs once supported. Nitro Forge does not replace
the upstream Worklets runtime or create QNX codegen support by wrapping it.

Paths below are relative to the named package at the pinned release above.

| Upstream seam                                                                 | Required QNX implementation / evidence                                                                                                                                                                           |
| ----------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Worklets `Common/cpp/worklets/Tools/UIScheduler.{h,cpp}`                      | Derive the actual upstream scheduler. Dispatch to the Qt GUI owner thread, preserve execution on that thread, and protect queued callbacks against destruction. `triggerUI()` drains the upstream job queue.     |
| Worklets `Common/cpp/worklets/Tools/JSScheduler.h`                            | Supply the real RN runtime, `CallInvoker`, and JS-queue identity predicate; do not access JSI from arbitrary Qt callbacks.                                                                                       |
| Worklets `Common/cpp/worklets/WorkletRuntime/RuntimeBindings.h`               | Supply frame callbacks, a logging host function and an audited networking backend/capability boundary. Use consistent monotonic frame timestamps.                                                                |
| Worklets `Common/cpp/worklets/NativeModules/WorkletsModuleProxyInitializer.h` | Connect preparation, `finalize` and `invalidate` to host bootstrap/reload/teardown before claiming worklet execution.                                                                                            |
| Reanimated `Common/cpp/reanimated/NativeModules/ReanimatedModuleProxy.h`      | Construct with the real UI WorkletRuntime, UIScheduler, RN runtime and CallInvoker; call `init` and `initializeFabric` with the host UIManager. Connect Fabric commit/mount hooks and layout-animation registry. |
| Reanimated `Common/cpp/reanimated/Tools/PlatformDepMethodsHolder.h`           | Implement frame, timestamp, UI update, flush, keyboard, sensor, pseudo-selector and gesture-state callbacks with truthful capability limits. `setGestureStateFunction` requires a real RNGH integration.         |

The optional CSS platform transition backend and animation factory may remain
null: the audited header explicitly retains the common C++ animation loop in
that case. This does not remove the other platform obligations.

Qt item access must remain on its owner thread. Runtime callbacks must stop
before Hermes teardown. A standalone frame timer or JS global is only a host
prerequisite, not proof that upstream Reanimated or Worklets runs. Preserve
unsupported status until real package execution, animation/event semantics,
reload/disposal and target builds are demonstrated. Keep host, QNX VM, QNX
hardware and Marathon/Wayland evidence separate.

The broader [exact-package audit](reanimated-gesture-handler-qnx-audit.md)
records the separate RNGH3.5.0 candidate and Expo/version/bootstrap decisions.
The older source snapshot below is historical context, not that selected version.

## Gesture Handler source contract

RNGH was inspected at
[`474bdf2c791bb807c119fa4d5f6a3ee984dc26d7`](https://github.com/software-mansion/react-native-gesture-handler/tree/474bdf2c791bb807c119fa4d5f6a3ee984dc26d7),
whose `packages/react-native-gesture-handler/package.json` declares `2.29.0`.
Its development dependencies include RN `0.88.0`, Reanimated `4.7.1` and Worklets
`0.13.0`; these are source context, not a QNX support declaration or instructions
to change this repository's dependency graph.

Paths below are relative to `packages/react-native-gesture-handler` at that pin.

| Source                                                                                                             | Port implication                                                                                                                                     |
| ------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `src/specs/NativeRNGestureHandlerModule.ts`                                                                        | Preserve upstream module registration, handler creation/update/attachment/drop and responder contracts; audit event payloads against the same pin.   |
| `shared/runtime/RNGHRuntimeDecorator.cpp`                                                                          | Shared runtime bindings are not a portable recognizer engine. Installing the decorator alone does not recognize gestures.                            |
| `android/src/main/java/com/swmansion/gesturehandler/core/PanGestureHandler.kt` and `GestureHandlerOrchestrator.kt` | Recognition and handler arbitration live in the platform implementation; a Qt adapter needs equivalent state transitions and relationship semantics. |
| `apple/Handlers/RNPanHandler.m`                                                                                    | Apple uses UIKit/AppKit pan recognizers; these cannot serve as QNX implementations.                                                                  |
| `src/RNGestureHandlerModule.windows.ts`                                                                            | Inspected fallback methods are no-ops, `gestures` is undefined and `moduleId` is `-1`. This is not a working desktop-native implementation to reuse. |

After input lifecycle correctness, the next bounded integration is a real
tap/pan slice: upstream JS/module contract, native handler state machine,
attachment/disposal, emitted state/update payloads, cancellation and interaction
with the Fabric responder. Prove it with actual JS listeners and native tests
before expanding recognizers or wiring worklet delivery. Qt gesture primitives
may help implementation, but their existence does not establish RNGH parity.

## Try the opt-in input lifecycle fixes

Use Linux with the repository's pinned Node/pnpm dependencies, Qt 6.11.1,
Clang, Ninja and CMake 3.31.10, as installed by
[the full host workflow](../.github/workflows/host-renderer.yml). The full
renderer build also needs its Hermes and React Native dependencies. This option
tests native input delivery; it does not enable Reanimated or RNGH.

From the repository root, run:

```bash
pnpm install --frozen-lockfile
cmake --preset linux-host-renderer -DRNQNX_ENABLE_INPUT_LIFECYCLE=ON
node scripts/validate-host-renderer.mjs
```

The validator builds the full renderer, checks that required tests exist and
runs CTest. If configuration reports missing Qt or dependencies, fix that
prerequisite; a smaller smoke-only test run is not equivalent. Inspect
`build/linux-host-renderer/evidence/` and
`build/linux-host-renderer/Testing/Temporary/` for test evidence. Record the
actual result separately; these commands are not a claim of a passing run.

To compare the default behavior or roll back the opt-in, configure with
`-DRNQNX_ENABLE_INPUT_LIFECYCLE=OFF` and rerun the validator. The CI matrix runs
both settings and retains separately named artifacts. Explicit lifecycle tests
exercise ON and OFF regardless of the matrix setting; the existing tests use
the build's default. A passing Linux run does not prove a QNX device run.

## Native guidance inspected

These are source-read records, not claims of skill installation or execution.
No new Nitro API or generated module is introduced by the input slice.

| Source / pin                                                                                                                                                                                                                      | Files read                                                                                                                                   | Decision applied                                                                                                                                                                                                  |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [Margelo react-native-skills](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be), `1e9e17be6f41c838db472d6a4b943fc26355a3be`                                                           | `skills/api-design/SKILL.md` → `skills/build-nitro-modules/SKILL.md` → `skills/cpp/SKILL.md`                                                 | Preserve upstream public APIs; keep native ownership/cancellation explicit; do not invent a Nitro facade or generated QNX bindings.                                                                               |
| [Callstack react-native-best-practices](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md), `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | `SKILL.md` (blob `86ec5ab5a4cadf454db8013293679891098efce1`), `references/native-threading-model.md`, `references/native-memory-patterns.md` | Keep UI mutation on its owner thread; avoid blocking input work and speculative thread assumptions; clean up listeners/resources; require measured interaction evidence before claiming latency/FPS improvements. |
| [Callstack github-actions](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/github-actions/SKILL.md), same Callstack pin                                                   | `SKILL.md` (blob `8f420a3328f8cfd0e85c6ee89e5b5a0c45beb39d`)                                                                                 | Retain retrievable artifacts with distinct ON/OFF names. The mobile simulator/emulator templates are not applicable to this Qt Linux host lane.                                                                   |
| [Callstack writing-user-docs](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/writing-user-docs/SKILL.md), same Callstack pin                                             | `SKILL.md` (blob `a2241b43014d7295f1439b5c869e2b8182d1ccd4`)                                                                                 | Put host prerequisites and support limits before the opt-in commands; explain missing-dependency failures and rollback. Applied to the usage section, not the internal source audit.                              |

For touch correctness, test multi-contact cancellation, stale-target cleanup,
coordinate conversion and a fresh gesture following cancellation. These tests
cannot establish RNGH arbitration or UI worklet support. Subsequent integration
needs pinned package builds plus real JS worklet/event tests and target runs;
retain a draft PR while required target evidence is absent.

## Implementation validation

The opt-in also connects the playground window, mounting adapter and responder
delegate to the dispatcher. See [the validation record](validation/qt-input-lifecycle-2026-11-07.md)
for the completed macOS ON/OFF suite and deterministic injected-input probe on
physical QNX. A later [JS-delivery proof](evidence/pi400-input-js-2026-11-07.md)
checks raw Fabric callbacks; USB/touch hardware, React component interaction and
Marathon/Wayland remain unverified. No general device input support is claimed.
