# Current state

Start with the [2026-11-13 handoff](HANDOFF_2026-11-13.md): it explains how to
load and control the app on the Pi 400, what is merged, and what is open.
The sections below keep their original dates.

## PR 105 review follow-up — 2026-11-10

The constrained Nitro generator at `c09a5b1` rejects unsupported contracts and
preserves the canonical generated bindings. Its hosted workflows passed;
the local generator, typecheck and installed-source checks passed again.
This follow-up changes documentation and structured evidence only. See
[the candidate receipt](../validation/pr105-merge-readiness-2026-11-10.md).
The historical platform baseline below and the seven original release gates are unchanged. Four owner gates (app identity, window chrome, Expo readiness, updated packages) were added on 2026-11-11 as pending.

Reconciled against `main` at `5f17504` on 2026-11-07. The structured dependency
baseline in [project state](../../.project-state.json) and the
[compatibility matrix](../getting-started/compatibility-matrix.md) are generated
from the package manifests and installed frozen graph. Run `pnpm validate:docs`
to check them; `pnpm docs:sync` updates the generated records after an intentional
change. Evidence is curated separately from dependency versions.

## What has run

| Environment                                                            | Result                                                                                                                    | Evidence                                                                                                      |
| ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| Physical Pi 400, custom BCM2711 image, QNX 8.0.5, Qt 6.12.0 evaluation | Fullscreen 1920×1080 Hello World through React Native, Fabric, Hermes and Qt Quick; synchronous Nitro v1 getter/add proof | [Hello World](../evidence/pi400-hello-world-2026-11-07.txt)                                                   |
| Same physical Pi 400                                                   | Optional Qt/Nitro callbacks, Promises, errors and teardown races                                                          | [Qt dispatcher](../evidence/pi400-nitro-qt-2026-11-07.md)                                                     |
| Same physical Pi 400                                                   | Ten supervised application launch/capture/exit trials and failure controls                                                | [Runtime baseline](../evidence/pi400-runtime-baseline-2026-11-07.md)                                          |
| Same physical Pi 400                                                   | Synthetic Qt input reaches raw Fabric JS callbacks                                                                        | [Input receipt](../evidence/pi400-input-js-2026-11-07.md)                                                     |
| Development host                                                       | Component and prop coverage; full native suite reported 16/16 in the dated receipt                                        | [Support table](../component-support.md), [runtime receipt](../evidence/pi400-runtime-baseline-2026-11-07.md) |

The Pi 400 Nitro device evidence (Hello World getter/add and Qt dispatcher) is for Nitro alpha 105 only; it was not re-run after the 136 re-pin (#119).

These are historical prerequisite results from the cited revisions. A new PR
must attach its own relevant validation. The raw Fabric callback input receipt does not establish USB
keyboard/touch behavior or real React responder behavior; the scoped Pressable
follow-up below covers the latter. Ten application trials do
not establish cold boot. Captures do not measure presentation timing.

## React Pressable responder prerequisite

Synthetic Qt input exercises the real production-bundled React Pressable
and responder path on Pi 400/QNX 8.0.5. The probe caught a stale responder after
unmount: RN's event path skipped cancellation when the native target was gone.
An exact, opt-in RN patch preserves nullable cancellation and error forwarding.
The same executable/HBC passes with the fix and reproduces the remount failure
when only that fix is disabled. Delivery interception also fails as expected;
a subsequent normal run succeeds. Readbacks matched both artifact hashes.

Fresh host graphs passed 23/23 with input/tracing ON and 16/16 with both OFF.
The existing input option stays OFF by default. The continuous Hello World app,
recovery boot and window contract are preserved. This is historical prerequisite
synthetic-input evidence, not validation of this integrated candidate or
physical HID, RNGH or Worklets qualification. See the
[responder receipt](../evidence/pressable-qnx-2026-11-07.md).

## Bounded native tracing prerequisite

The default-OFF trace experiment records actual host evaluation, surface
start/stop and owned queue admission/execution/close with bounded storage,
session-relative monotonic timestamps and explicit drops. Physical Pi 400
application export, protected-file failure, OFF-path capture and traced Nitro
worker/shutdown assertions passed. Host graphs passed 20/20 ON and 18/18 OFF,
followed by 4/4 focused tests after adding queue correlation assertions. See the
[trace receipt](../evidence/pi400-runtime-trace-2026-11-07.md) for exact candidate
identities and flags. The earlier 16/16 host result above remains dated evidence.

These are historical prerequisite native boundary diagnostics, not validation
of this integrated candidate. Input/Fabric/render-loop/presentation markers,
clock alignment, tracing overhead and performance budgets remain open. No
scheduling, boot, graphics or default-window policy changed.

## What still blocks v1

- Graphics starts manually on the Pi 400; recovery boot remains selected.
  Automatic graphics startup is disabled after failed boot tests.
- Stock Pi 4/5 QSTI 0.5.0 build 50 has no equivalent full-runtime receipt.
- Wider component behavior, physical input, IME/accessibility, and lifecycle
  coverage need named target results. See the separate device columns in the
  [support table](../component-support.md).
- Startup-to-first-frame, presented-frame interval and RSS are unmeasured.
- The reviewed baseline had hosted CI billing/spending refusals.
  [PR #68](https://github.com/mikevocalz/react-native-qnx/pull/68/checks) now starts
  hosted jobs; candidate results remain separate from local checks.
- Open-source Qt is the selected path, with no paid-license prerequisite, per
  [ADR-0013](../adr/0013-open-source-qt.md). The older evaluation artifacts expire
  on 17 November 2026; qualify the separate public-source build and its
  accompanying materials through [packaging](../packaging.md).
- The CLI and template are source-checkout tools. npm publication, generic
  Nitrogen QNX generation and a standalone native app template remain open.

The [v1 readiness record](../releases/v1-readiness.md) defines the remaining
acceptance evidence. `pnpm validate:v1` fails while any required gate is open.

## Continue work

Use [the QNX queue](QNX_VALIDATION_QUEUE.md) and
[the delivery plans](../platform-modernization/README.md) for the next runtime
slice. Preserve the existing fullscreen app, manual graphics stack and recovery
boot during bounded tests. Forge is an optional licensed consumer integration;
it is not a core platform dependency or a release gate.

Older statements, toolchains and commands remain in
[the dated handoff history](HISTORY_THROUGH_2026_11_07.md). They are historical
context, not the active support contract.

## PR #111 — Qt v2 CI coverage (2026-11-10)

The implementation candidate is `295f7240099c7371dc8988f4d411b1840888a415`
on `feat/v2-native-qt-apis`. The enabled-input hosted renderer lane now also
enables Qt SVG, motion and graphics; the other lane explicitly keeps all three
OFF. The validator requires the five feature CTests and rejects unexpected
registrations in an OFF lane. Deterministic SVG generation is checked before
the enabled native build. Product feature defaults and native APIs are unchanged.

The orchestration regression reproduced the former missing-test acceptance and
passes after the fix. Typechecking, SVG generation, formatting and documentation
validation pass. New hosted native execution is pending on this candidate; the
prior Linux runtime receipt remains scoped to its cited source. QNX and Marathon
execution for the v2 slice remain unrun. See [the v2 receipt](../v2/validation.md).
This handoff changes only records; implementation/workflow edits are committed
at the candidate above. No release gate is promoted.
