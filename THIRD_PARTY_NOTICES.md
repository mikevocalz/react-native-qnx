# Third-party notices and distribution boundary

The [MIT license](LICENSE) covers repository-authored code. Dependencies retain
their own copyright notices, licenses and distribution terms. This source
inventory is not a complete binary SBOM or permission to redistribute a target
runtime. A release must collect notices for its actual linked/plugin/font
closure, including transitive components.

| Component                                                | Source of terms                                                      | Packaging requirement                                                                                                                  |
| -------------------------------------------------------- | -------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| React and React Native                                   | Installed package `LICENSE` files (MIT)                              | Preserve upstream notices in distributions that contain them                                                                           |
| Hermes                                                   | Pinned Hermes source `LICENSE` (MIT), plus its external dependencies | Collect notices from the exact source/build closure                                                                                    |
| Nitro core, RN adapter and Nitrogen                      | Exact pinned packages and their license files                        | Preserve notices; generated/native dependencies need their own inventory                                                               |
| folly, fmt, glog, double-conversion, fast_float and Yoga | Each pinned source's license/notice files                            | Inventory the versions actually fetched or bundled by RN; root MIT does not replace their terms                                        |
| Qt                                                       | Selected open-source source/module licenses                          | Record the public-source build, every module/plugin, notices and applicable LGPL/GPL materials                                         |
| QNX                                                      | Licensed SDP/BSP/image and runtime terms                             | Obtain independently; do not include tools, sysroots, headers, vendor libraries, images or account/license material in public packages |
| Optional premium Forge packages                          | Separately licensed product terms                                    | Excluded from core platform distributions; follow [the consumer boundary](docs/policy/premium-components.md)                           |

Qt's [licensing documentation](https://doc.qt.io/qt-6/licensing.html) describes
commercial and open-source options and module-specific exceptions. Its
[QNX page](https://doc.qt.io/qt-6/qnx.html) names Device Creation Professional.
Open-source Qt is the selected path in [ADR-0013](docs/adr/0013-open-source-qt.md);
a paid subscription is not a project prerequisite. The private Pi 400 evaluation
recorded in [ADR-0011](docs/adr/0011-qt-module-licensing.md)
expires on 17 November 2026 and does not establish a production distribution
basis for those artifacts. Track the separate open-source build in
[packaging](docs/packaging.md).

The public-source Qt 6.12.0 build for QNX is recorded in
[its Pi 400 receipt](docs/evidence/pi400-public-qt-2026-11-12-b51b49c.md) and
[closure manifest](docs/evidence/qt-public-runtime-closure-2026-11-12-b51b49c.json).
Its runtime contains Qt Core, Gui, Network, OpenGL, Qml, QmlMeta, QmlModels,
QmlWorkerScript, Quick and QuickShapes, the QNX QPA plugin and five QML
plugins, all under `LGPL-3.0-only` (with GPL and commercial alternatives).
The compiler also read bundled code with its own terms, found by following
every header and `#include`d source:

| Component                                            | Terms                                                                       | Reaches the build through                |
| ---------------------------------------------------- | --------------------------------------------------------------------------- | ---------------------------------------- |
| FreeType 2.14.3 (and `qgrayraster.c`)                | FTL (chosen from `FTL OR GPL-2.0-only`); Zlib, MIT parts                    | Qt Gui font engine                       |
| HarfBuzz 14.5.0                                      | MIT                                                                         | Qt Gui text shaping                      |
| libpng 1.6.58                                        | Libpng AND libpng-2.0                                                       | Qt Gui image I/O                         |
| zlib 1.3.2                                           | Zlib                                                                        | Qt Core compression                      |
| PCRE2 10.48 and SLJIT                                | BSD-3-Clause with binary-package exception; BSD-2-Clause                    | `QRegularExpression`                     |
| double-conversion 3.4.0                              | BSD-3-Clause                                                                | Qt Core number formatting                |
| libpsl and the Public Suffix List data               | BSD-3-Clause; MPL-2.0 (data)                                                | Qt Network                               |
| MD4C 0.5.3                                           | MIT                                                                         | Markdown reader                          |
| TinyCBOR 7.0                                         | MIT                                                                         | Qt Core CBOR                             |
| SHA-1, MD4, MD5, SHA-3, RFC 6234 SHA-384/512, BLAKE2 | public domain, CC0-1.0, BSD-2-Clause, BSD-3-Clause, `CC0-1.0 OR Apache-2.0` | `#include` from `qcryptographichash.cpp` |
| Robert Penner easing equations                       | BSD-3-Clause                                                                | `#include` from `qeasingcurve.cpp`       |
| SipHash                                              | CC0-1.0                                                                     | `#include` from `qhash.cpp`              |
| forkfd                                               | MIT                                                                         | `#include` from `forkfd_qt.c`            |
| emoji-segmenter 0.4.0 (Copyright 2019 Google LLC)    | Apache-2.0; no license file in its directory                                | `#include` from `qtextengine.cpp`        |
| Unicode UCD and CLDR data                            | Unicode-3.0                                                                 | Qt Core text and locale tables           |
| `tl::expected`                                       | CC0-1.0                                                                     | Qt Core header                           |
| Khronos OpenGL ES 2 headers                          | MIT                                                                         | Qt Gui OpenGL                            |
| Adobe Glyph List                                     | BSD-3-Clause                                                                | `qfontsubset_agl.cpp`                    |
| Imlib2-derived smooth scaling                        | BSD-2-Clause AND Imlib2                                                     | `qimagescale.cpp`                        |
| X Server region helper, XSVG arc code                | X11 AND HPND; HPND-sell-variant                                             | `qregion.cpp`, `qguisvg.cpp`             |
| JavaScriptCore macro assembler                       | BSD-2-Clause                                                                | Qt Qml                                   |

The runtime's `licenses/` directory holds those texts, and
`licenses/THIRD_PARTY_ATTRIBUTIONS.txt` lists each component's copyright line
and license, including emoji-segmenter, whose Apache-2.0 notice ships there
with the module's `Apache-2.0.txt`. [Packaging](docs/packaging.md) lists what
must ship with the runtime, including the FreeType credit line.

For the selected LGPL route, document the corresponding
source, notices, library replacement/relinking and installation obligations
that apply to that release. Tie those materials to the exact source and build
closure. Keep evaluation and licensed vendor
payloads out of npm tarballs, public artifacts and this repository.
