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
