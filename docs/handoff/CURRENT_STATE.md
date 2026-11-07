# Current state

Reconciled against `main` at `be71bc9` on 2026-11-07. The structured dependency
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
must attach its own relevant validation. Synthetic input does not establish USB
keyboard/touch behavior or React responder parity. Ten application trials do
not establish cold boot. Captures do not measure presentation timing.

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
