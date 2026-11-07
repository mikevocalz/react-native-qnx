# Packaging and Licensing Boundary

This document records constraints, not legal advice. No QNX, Qt, or npm package
has been produced by the current working tree.

## Project code

Repository-authored code is MIT-licensed. Third-party source and generated
artifacts retain their own notices and obligations; the project license does not
replace them.

## QNX

The QNX Everywhere introduction offers a free, non-commercial QNX SDP 8.0
license through a signed-in myQNX account. Commercial development or shipping
requires a separate license determination with QNX.

QNX SDP tools, sysroots, headers, libraries, target images, BSP payloads,
licenses, and account data are not committed or redistributed here. Public CI
cannot claim a QNX build unless it runs on an authorized private/self-hosted
runner and records only sanitized evidence.

Source: [QNX Everywhere introduction and license acquisition](https://www.qnx.com/developers/docs/qnxeverywhere/introduction.html).

## Qt module posture

ADR-0011 records the per-module licensing gate. Under the open-source (LGPLv3)
posture, the following are excluded unless the owner moves to a Qt commercial
license:

- `Qt Virtual Keyboard` — GPLv3-only open-source; required for on-screen
  keyboard on touchscreen targets.
- `Qt Quick 3D`, `Qt Graphs`, `Qt Lottie Animation`, `Qt Quick Timeline`,
  `Qt Quick 3D Physics`, `Qt Canvas Painter`, `Qt Qml Compiler`, `Qt HTTP Server`,
  `Qt MQTT`, `Qt OPC UA` — all GPLv3-only in the open-source edition.

All other modules are presumed available under LGPLv3 open-source terms, subject
to re-audit against the pinned Qt LTS source.

## Qt for QNX stop gate

The platform specification assumes Qt 6 can be built from source for the Everywhere target under
LGPLv3 obligations. Qt's general licensing page documents LGPLv3 availability
for eligible Qt components, but the current Qt-for-QNX page states that Qt for
QNX is available under Qt for Device Creation Professional.

The repository therefore will not download, build, deploy, package, or
redistribute Qt for QNX until the owner obtains a documented determination for
the exact QNX Everywhere/non-commercial scenario. When resolved, ADR-0001 and
ADR-0011 must record:

- the authorized Qt source/distribution channel and exact tag;
- commercial or open-source license basis for each required module;
- dynamic-link/relink and corresponding-source obligations where applicable;
- plugin, QML, font, and third-party notice packaging;
- modules excluded because their license or QNX support is incompatible.

Sources: [Qt for QNX](https://doc.qt.io/qt-6/qnx.html) and
[Qt licensing](https://doc.qt.io/qt-6/licensing.html).

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

## npm packages

`@rn-qnx` is provisional. Do not publish until the owner supplies the real npm
organization, package ownership, provenance/authentication policy, and
CODEOWNERS identities. Independent release-please units and per-package
changelogs begin only after real package manifests exist.

## Release evidence

A release claiming QNX support requires, at minimum:

1. frozen dependency/source identities;
2. a licensed qcc cross-build on the supported host;
3. execution of the phase acceptance binary/app on the named QNX target;
4. cited installed/pinned native seams;
5. generated Nitrogen output when a HybridObject spec changes;
6. updated support/gap tables and an honest list of unimplemented capabilities.
