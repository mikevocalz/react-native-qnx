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

ADR-0011 records module terms. The following stay outside the selected
LGPL-compatible core; adding them requires a compatible GPL product decision
or an independently chosen commercial route:

- `Qt Virtual Keyboard` — GPLv3-only open-source; required for on-screen
  keyboard on touchscreen targets.
- `Qt Quick 3D`, `Qt Graphs`, `Qt Lottie Animation`, `Qt Quick Timeline`,
  `Qt Quick 3D Physics`, `Qt Canvas Painter`, `Qt Qml Compiler`, `Qt HTTP Server`,
  `Qt MQTT`, `Qt OPC UA` — all GPLv3-only in the open-source edition.

Check every selected module against the pinned source notices; do not infer
terms for uninspected modules.

## Open-source Qt packaging

The owner selected the open-source Qt route in
[ADR-0013](adr/0013-open-source-qt.md). A paid Qt subscription is not a platform
prerequisite. Marathon uses the Linux/Wayland target; QNX uses its own toolchain
and runtime. Keep those target and artifact records separate.

Proceed with eligible public Qt sources and record the exact source channel,
tag/hash, module/QPA license files and built library/plugin closure. Include
the notices, corresponding source and library replacement/relinking materials
required by the selected terms. The existing LGPL-compatible module subset
stays in place; optional GPL-only modules need an explicit compatible product
choice. A commercial Qt subscription is one optional route, not the default
action or an approval stop for open-source development.

Qt's [general licensing page](https://doc.qt.io/qt-6/licensing.html) documents
the open-source option; [the LGPL terms](https://doc.qt.io/qt-6/lgpl.html) describe
the associated conditions. The [QNX commercial product page](https://doc.qt.io/qt-6/qnx.html)
does not replace the per-file/source audit for the selected open-source build.

### Public-source Qt 6.12.0 for QNX (2026-11-12)

The first public-source build is recorded in
[the Pi 400 public Qt receipt](evidence/pi400-public-qt-2026-11-12-b51b49c.md),
with every runtime file's SHA-256 in
[the closure manifest](evidence/qt-public-runtime-closure-2026-11-12-b51b49c.json).

- **Source:** `qt-everywhere-src-6.12.0.tar.xz` from
  `download.qt.io/official_releases/qt/6.12/6.12.0/single/`, SHA-256
  `98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165`, MD5
  `ac677cc64158740f257fd37a5b4c5ffb`. No 6.12.1 was published on 2026-11-12.
  The three published qtbase 6.12 security diffs (`CVE-2026-116337` 0001 and
  0002, `CVE-2026-105924`) are applied to `src/` before building.
- **Modules:** qtbase (Core, Gui, Network, OpenGL), qtdeclarative (Qml,
  QmlMeta, QmlModels, QmlWorkerScript, Quick, QuickShapes) and the QNX QPA
  plugin `plugins/platforms/libqqnx.so`, plus the QtQml, Models, WorkerScript,
  QtQuick and Shapes QML plugins. qtshadertools is a host build tool only.
  This is the evaluation build's subset; nothing was added.
- **Terms:** the audit covers every file the compiler read, headers and
  `#include`d sources included. Every Qt module file offers `LGPL-3.0-only`
  and no file is GPL-only. Bundled third-party code (20 `3rdparty`
  components plus attributed code inside Qt sources) is permissive, public
  domain, CC0, Apache-2.0 (emoji-segmenter, BLAKE2 option), MPL-2.0 data
  (Public Suffix List) or, for FreeType, `FTL OR GPL-2.0-only` with the FTL
  taken. The receipt lists each component.
- **Recipes:** [`scripts/qt-public/`](../scripts/qt-public/) holds the
  extract, patch, host-tool and QNX configure scripts, the toolchain file,
  the runtime staging script that checks the `DT_NEEDED` closure, the SPDX
  audit and the device launchers. Its [README](../scripts/qt-public/README.md)
  gives the expected container layout and run order.

#### Material to ship with a runtime built this way

Per [Qt's LGPL page](https://doc.qt.io/qt-6/lgpl.html) and LGPL-3.0, a package
that includes these Qt libraries carries:

1. **Notices:** the staged `licenses/` tree (LGPL-3.0-only and GPL texts from
   each module's `LICENSES/`, every bundled third-party LICENSE, COPYING,
   LICENCE and FreeType `FTL.TXT` file, and the license files named by the
   `qt_attribution.json` entries the audit found), plus
   `licenses/THIRD_PARTY_ATTRIBUTIONS.txt`. That file names each bundled
   component with its copyright line and SPDX id, which covers components
   that ship no license file of their own, such as emoji-segmenter
   (Apache-2.0, Copyright 2019 Google LLC), MD4, MD5, SipHash, SHA-1 and the
   Unicode data. Add a statement that the application uses Qt under
   LGPL-3.0.
2. **FreeType credit:** the FTL requires this line in the product's
   documentation, not only in the license directory: "Portions of this
   software are copyright © 2025 The FreeType Project (https://freetype.org).
   All rights reserved." The year is the one in bundled FreeType 2.14.3's
   `include/freetype/freetype.h` (`1996-2025`); update it with FreeType.
3. **Corresponding source:** the exact archive above, identified by URL and
   checksum, the three security diffs with their checksums, and the scripts in
   `scripts/qt-public/`. If the package is distributed, offer that source from
   the same place or with a written offer, as GPL-3.0 section 6 requires for
   the library's object code (LGPL-3.0 incorporates the GPL). Relying on a
   third-party mirror requires that mirror to keep serving that source.
4. **Library replacement:** Qt is linked as shared libraries
   (`libQt6*.so.6`), so a user can rebuild Qt from the source above with the
   same configure line and put the result on `LD_LIBRARY_PATH`, or replace the
   files in the runtime directory. The Pi 400 receipt shows this path working:
   application binaries linked against the evaluation libraries ran
   unchanged on the public-source libraries.
   Shipped application binaries must not carry a build-host `DT_RPATH`. The
   current RN executables have
   `/home/qnxbuild/qt612-qnx/qtbase/lib:/home/qnxbuild/rn-build/hermes/lib`,
   and QNX's loader searches `DT_RPATH` before `LD_LIBRARY_PATH`. On the Pi
   the replacement worked only because those directories do not exist there;
   on a machine where they do, the user's library would be ignored. Strip the
   RPATH at link or install time, or set it to a relative `$ORIGIN` path
   inside the package, so `LD_LIBRARY_PATH` or the runtime directory decides
   which Qt is loaded. For a locked-down device
   ("User Product" under LGPL-3.0 section 4e and GPL-3.0 section 6), the
   installation information needed to run a modified library must also be
   supplied.

Keep the runtime package free of QNX SDP files (sysroot, headers,
`libscreen`, `libEGL`, `libc++` and other system libraries from the board),
the Pi 400 Mesa/libdrm overlay under `/data/pi400-graphics-compat` (QNX
Software Center payloads), evaluation artifacts and account or license
material. The closure manifest lists those QNX libraries as dependencies
only.

No package has been produced yet. The public runtime currently exists only as
a local build output and the board copy under the test user's home.

## Pi 400 evaluation authorization (2026-11-06)

The owner requested the free ten-day Qt evaluation. The signed-in commercial
Qt portal was inspected and showed **Qt for Device Creation Enterprise,
Evaluation, Active**, expiring **17 November 2026**. This records a temporary
evaluation basis for the private Pi 400 bring-up, not a production or
redistribution authorization for those artifacts.
[ADR-0013](adr/0013-open-source-qt.md) selects the separate open-source Qt path.

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
authorize release packaging. The expiry applies to these evaluation artifacts; it does not expire
independently obtained open-source Qt.

Sources: [Qt account licenses](https://account.qt.io/s/),
[Device Creation tiers](https://www.qt.io/development/qt-framework/target-devices),
[Qt for QNX](https://doc.qt.io/qt-6/qnx.html).

### Evaluation deployment subset

ADR-0011's restricted evaluation decision also records QmlMeta, Qml Models,
Qml WorkerScript and Quick Shapes, required by the existing Qt Quick runner.
The source headers were checked against the acquired 6.12.0 archive. Only
selected build targets and their actual runtime closure will be deployed;
Quick Controls, Quick Layouts, Widgets and other optional modules remain
outside this application deployment. Binary size, target build time and
rendered-window proof remain pending.

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

### Observed OpenGL runtime dependency

The 6.12.0 QNX build also produces Qt OpenGL from qtbase, required by the
selected Quick rendering path. It is included in this restricted private
evaluation runtime. `qtbase/src/opengl/qopenglshaderprogram.cpp` offers
`LicenseRef-Qt-Commercial` alongside LGPLv3/GPL alternatives. The built QNX QPA
plugin is `plugins/platforms/libqqnx.so`; QML plugin filenames and the shared
library closure were inspected from the built artifacts, rather than assumed.
