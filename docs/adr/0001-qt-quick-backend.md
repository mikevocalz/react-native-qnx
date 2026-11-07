# ADR-0001: Qt Quick as the QNX component backend

## Status

Accepted architecture; G0-B implementation is blocked pending real toolchain,
target, source, and licensing evidence.

## Context

The platform needs a retained native item tree for Fabric component views,
shaped text, images, input, animation, and accessibility. The specification locks Qt
Quick as that backend so the renderer, Fabric host, Hermes host, and Nitro C++
layer share one native language and event-loop model.

QNX Screen remains the window-system/compositor API beneath Qt's QNX Platform
Abstraction plugin. The existing direct-Screen executable is therefore a G0
window-system probe and a possible future bare-Screen experiment, not the React
Native component backend.

GTK4 was technically viable rather than unavailable: the QNX Everywhere June
2026 release notes document functioning GTK application windows and a GTK4 demo
on the Developer Desktop. It is rejected here because the locked architecture
uses a C++ end-to-end runtime and Qt Quick item tree rather than a second GObject
object/event model.

## Decision

Use Qt Quick for React Native component views. Yoga remains the only layout
authority; Qt items are positioned as absolute leaves and do not use Qt anchors
or layout managers to reinterpret the Fabric/Yoga result.

The first implementation may begin only after G0-A and G0-D pass. G0-B must
then:

1. select an exact Qt 6 LTS source tag proven with the installed SDP;
2. cite the QNX QPA implementation and toolchain support from that checkout;
3. cross-build the required `qtbase` and `qtdeclarative` subset;
4. identify the built platform plugin and runtime files rather than assuming
   names;
5. render a minimal Qt Quick window on the Pi over HDMI.

No private Qt header is authorized by this ADR.

## Licensing gate

The specification proposes an LGPLv3 source-build posture. Qt's general licensing page
describes LGPLv3 use when its obligations are met, while the current Qt-for-QNX
product page states that Qt for QNX is available under Qt for Device Creation
Professional. This discrepancy is a stop-and-ask trigger.

Before G0-B downloads, builds, deploys, or distributes Qt for QNX, the repository
owner must obtain a documented licensing determination for this exact
non-commercial QNX Everywhere use. This ADR does not interpret either license or
claim that an open-source source build bypasses the Qt-for-QNX product terms.

## Pi 400 evaluation authorization (2026-11-06)

The owner requested the free ten-day Qt evaluation. The signed-in commercial
Qt portal was inspected and showed **Qt for Device Creation Enterprise,
Evaluation, Active**, expiring **17 November 2026**. This records a temporary
evaluation basis for the private Pi 400 bring-up, not a production or
redistribution authorization. The open-source QNX licensing question remains
unresolved outside this evaluation.

The licensed account Downloads page supplied Qt **6.12.0 LTS**, full-framework
UNIX source archive `qt-everywhere-src-6.12.0.tar.xz`, SHA-256
`98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165`.
Its top-level `.tag` is `ef25002ed292b132d1ed3246bfcee1d24a9cd966`.
The archive also matches the portal-provided `md5sums-6.12.0` value
`ac677cc64158740f257fd37a5b4c5ffb`. These are integrity checks, not an
independently verified vendor signature. Account identifiers, license numbers, credentials, and licensed
source are excluded from the repository.

Only qtbase (Core, Gui, Network), qtdeclarative (Qml, Quick), and qtshadertools
(matching host shader tools) are selected for this experiment. Their exact
source notices, QNX support, built dependencies, and deployment files must be
checked before the target runtime is accepted. Other optional modules remain
excluded. The evaluation does not accept this ADR's broader module matrix or
authorize release packaging. After expiry, further licensed use requires a
valid license determination.

Sources: [Qt account licenses](https://account.qt.io/s/),
[Device Creation tiers](https://www.qt.io/development/qt-framework/target-devices),
[Qt for QNX](https://doc.qt.io/qt-6/qnx.html).

## Consequences

- Fabric component views will own Qt Quick items rather than direct Screen
  buffers or Skia drawing primitives.
- QNX Screen lifecycle tests remain useful below QPA but cannot validate the Qt
  component layer.
- Qt runtime deployment, plugin discovery, fonts, LGPL relinkability, and module
  licensing become explicit packaging concerns.
- Qt Multimedia is not assumed: current Qt QNX support documentation lists it as
  unsupported with SDP 8.0.

## Alternatives considered

- **GTK4:** viable on the Everywhere desktop, but rejected by the locked design
  for C++ object/event-loop coherence.
- **Direct Screen plus a custom renderer:** rejected as the component backend;
  it would recreate text, scene graph, image, input, and accessibility systems.
- **Skia as the component renderer:** superseded by this decision. Skia may be a
  future leaf/canvas capability only after a separate source-backed decision.

## Evidence and sources

- [Qt for QNX support](https://doc.qt.io/qt-6/qnx-support.html)
- [Qt for QNX licensing statement](https://doc.qt.io/qt-6/qnx.html)
- [Qt licensing](https://doc.qt.io/qt-6/licensing.html)
- [QNX Everywhere June 2026 release notes](https://www.qnx.com/developers/docs/qnxeverywhere/rel_notes.html)
- Platform specification §§3–6

Installed/pinned Qt source paths and symbols remain intentionally absent until
G0-B acquires and pins that source tree.
