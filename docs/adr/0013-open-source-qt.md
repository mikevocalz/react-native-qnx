# ADR-0013: Use open-source Qt without a paid-license prerequisite

## Status

Accepted project direction, 2026-11-07, following the repository owner's
clarification. Public-source build qualification remains an implementation
step; this decision does not relabel the earlier evaluation artifacts.

## Decision

Use eligible open-source Qt components under their LGPL/GPL terms. A paid Qt
subscription is not a prerequisite for this project's open-source development
or its Marathon Linux/Wayland target. Keep the project-authored code under MIT.
The root LICENSE file grants that open-source license; it is not a purchase.

Qt's [licensing page](https://doc.qt.io/qt-6/licensing.html) documents an LGPLv3
option for eligible components, and [the LGPL terms](https://doc.qt.io/qt-6/lgpl.html)
cover notices and the applicable source/library replacement requirements.
Commercial Qt offerings are a separate choice. Do not use a revenue threshold
or the project's non-commercial label as a substitute for the selected source
and component terms.

For the QNX target, proceed with an eligible public-source build and inspect
its exact module/QPA notices, source channel, hashes, shared libraries and
plugins as part of packaging. The [Qt QNX product page](https://doc.qt.io/qt-6/qnx.html)
describes Device Creation Professional; its commercial offering is not treated
here as a blanket requirement to buy Qt before inspecting or building eligible
open-source components. The release record tracks the actual source build and
its accompanying materials, rather than a paid-license purchase.

QNX SDP/BSP/runtime terms are separate from Qt and from Marathon's Linux
runtime. Preserve the existing QNX Everywhere toolchain and non-redistribution
boundaries. Do not put vendor SDK payloads into platform source packages.

## Superseded wording and remaining work

This replaces the blanket Qt download/build/deploy stop in ADR-0001 and
packaging guidance. No additional owner confirmation is required to pursue
the selected open-source Qt path. Continue normal source/module and build
verification instead of stopping development to request a commercial license.

The earlier Pi 400 Qt 6.12.0 receipts used the recorded private evaluation.
Their 17 November 2026 expiry belongs to those evaluation artifacts; it does
not expire independently obtained open-source Qt. Produce a separately
identified public-source build before claiming it as a release artifact.
Keep the existing module subset; add optional modules only with their source
terms and target support recorded. Do not silently add GPL-only dependencies
to the selected LGPL-compatible core build.

[Packaging](../packaging.md) and [v1 readiness](../releases/v1-readiness.md)
record those artifact and workflow steps. Runtime, component and performance
gates remain independent of the choice to use open-source Qt.
