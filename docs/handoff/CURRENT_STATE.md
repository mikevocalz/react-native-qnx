# Current state

## 2026-11-10 — PR #110 receives its own SDK validation

The follow-up to `73f7da0` removes only the SDK workflow's PR base filter so
#110's stacked base receives full SDK checks. Native sources and evidence are
unchanged; the new published head's CI remains pending.
[Receipt](../validation/pr110-merge-readiness-2026-11-10.md).

## 2026-11-10 — PR #110 integrates the PR #106 public API

Combined published #110 `2657620` with #106 `5e3bb88` in a merge preserving
both histories. Public imports retain Image and I18nManager; Switch retains
the style array and controlled reconciliation. Moved style/prop/event/ref
assertions into the existing retained-hook harness after reproducing the old
mock's `useMergeRefs` failure. Both focused tests, both reset-cache Metro
bundles and typecheck pass. Native CI for this combined candidate is pending.

Earlier entries remain revision-specific history. The owner QNX report does
not cover this newly combined tree, and all seven release gates stay pending.
[Combined validation receipt](../validation/pr110-merge-readiness-2026-11-10.md).

## 2026-11-10 — PR #110 integration follow-up

Implementation `083c4bd` is being combined with the published #104 follow-up
`c5c54a0` and expanded host-renderer CI coverage. The original PR author reports
23/23 native CTests with the v1 options ON and 17/17 OFF, with the exclusions
and host limitation recorded in the receipt. Current focused host-renderer
inventory, controlled-Switch JavaScript and type checks passed; full native
CI on the updated candidate is pending. The two feature guides now retain
this evidence instead of saying nothing has been tested.

The owner-reported smooth QNX operation remains in the inherited #104 receipt;
it does not supply exact artifacts or enabled-feature acceptance. All seven
v1 gates remain pending. Receipt:
[pr110-merge-readiness-2026-11-10.md](../validation/pr110-merge-readiness-2026-11-10.md).

## 2026-11-10 — PR #104 readiness follow-up

The owner reports smooth QNX operation for PR #110. That inspected branch contains PR #104 at 3b414f1; the report does not specify the installed hashes, board/image, flags or actions. Retain it as owner-reported feedback alongside the existing host receipt. Corrected the stale Modal host note to reflect Escape/onRequestClose and viewport/background-input behavior without promoting device rows.

Receipt: [gallery-owner-report-2026-11-10.md](../evidence/gallery-owner-report-2026-11-10.md).

## PR 106 public-API review — 2026-11-10

The repaired Image methods and Switch style composition at `6b528c1` have
green hosted native/public-import checks. Public-wrapper tests and both real,
reset-cache Metro bundles pass again locally. This follow-up updates records
only. [Candidate receipt and PR 110 integration notes](../validation/pr106-merge-readiness-2026-11-10.md).
Physical QNX/Marathon qualification and all seven v1 gates remain separate.

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
