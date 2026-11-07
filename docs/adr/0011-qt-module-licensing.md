# ADR-0011: Qt module licensing gate

## Status

Accepted only for the restricted Pi 400 evaluation subset recorded below.
The broader open-source module matrix remains Proposed (2026-08-14). Its
historical evidence is from the installed Qt 6.11.1 Homebrew kegs and
Qt's online documentation; the ADR must be re-audited against the pinned Qt LTS
source tree once that source is available in the workspace.

## Context

ADR-0001 committed the project to a Qt 6 build-from-source posture for QNX.
Every Qt module is a licensing decision, not only a technical one. Some modules
are GPLv3-only under the open-source offering, while others are available under
LGPLv3. Adding a GPLv3-only module to an LGPLv3 product silently changes the
licensing posture.

The current macOS workspace has no pinned Qt source tree; only the Homebrew
6.11.1 binary kegs are present. Those kegs do not ship the source `LICENSES/`
directories or per-module `SBOM` artifacts that the project wants to cite, so
this record is an initial gate based on Qt's public documentation and the
installed keg inventory. It is marked Proposed until the pinned source can be
checked.

## Decision

1. A Qt module may enter the build only after its license is recorded in this
   ADR and `docs/packaging.md`.
2. Under the open-source (LGPLv3) posture, **do not build or ship any module
   listed by Qt as GPLv3-only** without a documented product decision to move
   to a commercial license or accept the GPLv3 obligations.
3. `Qt Virtual Keyboard` is the critical example. It is the on-screen keyboard
   for touchscreen targets, but it is open-source-only under GPLv3
   (`doc.qt.io/qt-6.11/licensing.html`). For the open-source posture it is
   **excluded**. The product must therefore choose one of:
   - acquire a Qt commercial license for Virtual Keyboard (and any other
     GPLv3-only modules);
   - build an on-screen keyboard as a React Native component over `<TextInput>`;
   - require a physical keyboard for text input.
     This is recorded as a product-level gap until the owner makes that choice.
4. All other modules are presumed available under the LGPLv3 open-source
   license, subject to confirmation against the pinned source tree.
5. No Tier 1 or Tier 2 module from Prompt 04 enters the build before this ADR
   is Accepted.

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

### Source audit and restricted runtime subset

The private 6.12.0 evaluation build includes Core, Gui, Network, Qml, Quick,
and their QmlMeta/Models/WorkerScript dependencies. Quick Shapes is also
required: the existing playground's View factory imports `QtQuick.Shapes`.
This records that existing dependency explicitly, rather than rewriting the
renderer to remove it. Shader Tools supplies matching build-time shader
processing. No other Tier 1/2 adoption is accepted by this evaluation record.

The exact 6.12.0 checkout headers in `qtbase/src/corelib/global/qglobal.h`,
`qtbase/src/gui/kernel/qguiapplication.cpp`,
`qtbase/src/network/kernel/qnetworkinterface.cpp`,
`qtdeclarative/src/qml/qml/qqmlengine.cpp`,
`qtdeclarative/src/quick/items/qquickitem.cpp`,
`qtdeclarative/src/qmlmodels/qqmlobjectmodel.cpp`,
`qtdeclarative/src/quickshapes/qquickshape.cpp`, and
`qtshadertools/src/shadertools/qshaderbaker.cpp` all offer
`LicenseRef-Qt-Commercial` alongside their open-source alternatives.
The checked top-level `LICENSES/LicenseRef-Qt-Commercial.txt` refers to the
licensee's applicable written/accepted agreement; the active evaluation is
the basis here. This does not resolve production or redistribution rights.

Qt's QNX implementation is
`qtbase/src/plugins/platforms/qnx/{CMakeLists.txt,qqnxintegration.cpp,qnx.json}`.
The source declares `QQnxIntegrationPlugin`, output `qqnx`, type `platforms`,
key `qnx`, linking Screen and optional EGL. The actual built artifact and
runtime dependencies remain to be verified. Its reference AArch64 toolchain
is `coin/provisioning/common/shared/cmake_toolchain_files/qnx-toolchain-aarch64le.cmake`.
The experiment uses system version 800 and preserves `_QNX_SOURCE`.

## Consequences

Clearer: every module adoption becomes reviewable and tied to license evidence.
Slower: we cannot ship a touchscreen `<TextInput>` with an on-screen keyboard
under the open-source posture.

## Alternatives considered

- **Assume all installed Homebrew modules are LGPLv3.** Rejected: the installed
  kegs do not carry the source license files, and Qt's documentation explicitly
  calls out GPLv3-only modules.
- **Adopt Virtual Keyboard under LGPLv3 anyway.** Rejected: it is not offered
  under LGPLv3 in the open-source edition and would change the product's
  licensing posture.
- **Wait for the pinned source before drafting any ADR.** Rejected: the owner
  asked to do all possible on the macOS host; documenting the current best
  evidence and the open questions is better than leaving the gate unwritten.

## Per-module evidence (Qt 6.11.1)

Sources:

- [Qt 6.11 licensing page](https://doc.qt.io/qt-6.11/licensing.html)
- [Qt for QNX](https://doc.qt.io/qt-6/qnx.html)
- Homebrew Qt 6.11.1 kegs under `/opt/homebrew/Cellar` (inventory only)

### Tier 0 — mandatory, already in build

| Module            | macOS installed                                                | Open-source license (per Qt 6.11 docs) | QNX build support | Notes                                                                                                                   |
| ----------------- | -------------------------------------------------------------- | -------------------------------------- | ----------------- | ----------------------------------------------------------------------------------------------------------------------- |
| Qt Core           | qtbase                                                         | LGPLv3 / GPLv2                         | presumed          | `FindSlog2.cmake` and `FindPPS.cmake` are present in `qtbase/lib/cmake/Qt6`; QNX symbols exist in Qt source.            |
| Qt GUI            | qtbase                                                         | LGPLv3 / GPLv2                         | presumed          | `qqnx_imf` and `slog2`/`qqnx_pps` appear in `mkspecs` disabled-features on macOS, confirming QNX branches in Qt source. |
| Qt Qml            | qtdeclarative                                                  | LGPLv3 / GPLv2                         | presumed          | core to ADR-0002.                                                                                                       |
| Qt Quick          | qtdeclarative                                                  | LGPLv3 / GPLv2                         | presumed          | component backend.                                                                                                      |
| Qt Quick Controls | qtdeclarative? (controls live in `qtdeclarative` since Qt 6.3) | LGPLv3 / GPLv2                         | presumed          | `TextInput`, `Switch`, etc.                                                                                             |
| Qt Network        | qtbase                                                         | LGPLv3 / GPLv2                         | presumed          | image loading, fetch.                                                                                                   |
| Qt Shader Tools   | qtshadertools                                                  | LGPLv3 / GPLv2                         | presumed          | build-time shader tooling.                                                                                              |

### Tier 1 — component-backing, adopt only when the feature lands

| Module              | macOS installed                              | Open-source license (per Qt 6.11 docs) | QNX build support  | Notes                                                           |
| ------------------- | -------------------------------------------- | -------------------------------------- | ------------------ | --------------------------------------------------------------- |
| Qt Quick Effects    | qtquickeffectmaker / `qtdeclarative` effects | LGPLv3 / GPLv2                         | presumed           | `filter`, blur, drop-shadow.                                    |
| Qt SVG              | qtsvg                                        | LGPLv3 / GPLv2                         | presumed           | SVG file rendering.                                             |
| Qt Quick Shapes     | in Qt Quick (qtdeclarative)                  | LGPLv3 / GPLv2                         | presumed           | vector paths.                                                   |
| Qt Multimedia       | qtmultimedia                                 | LGPLv3 / GPLv2                         | verify QNX backend | video, audio, camera.                                           |
| Qt Quick Dialogs    | qtquickdialogs                               | LGPLv3 / GPLv2                         | verify             | native file/color/dialogs.                                      |
| Qt Image Formats    | qtimageformats                               | LGPLv3 / GPLv2                         | presumed           | extra image codecs.                                             |
| Qt SQL              | qtsql (in qtbase)                            | LGPLv3 / GPLv2                         | verify             | local SQL.                                                      |
| Qt Virtual Keyboard | qtvirtualkeyboard                            | **GPLv3**                              | —                  | **Excluded** from open-source build. Product decision required. |
| Qt Qml Models       | qtdeclarative                                | LGPLv3 / GPLv2                         | presumed           | model/view helpers.                                             |
| Qt Concurrent       | qtbase                                       | LGPLv3 / GPLv2                         | presumed           | off-thread work.                                                |

### Tier 2 — evaluate per concrete need

| Module              | macOS installed      | Open-source license (per Qt 6.11 docs) | QNX build support   | Notes                                                                                      |
| ------------------- | -------------------- | -------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------ |
| Qt Quick 3D         | qtquick3d            | **GPLv3**                              | —                   | Large; excluded from open-source build unless commercial.                                  |
| Qt Graphs           | qtgraphs             | **GPLv3**                              | —                   | Excluded from open-source build unless commercial.                                         |
| Qt Lottie Animation | qtlottie             | **GPLv3**                              | —                   | Excluded from open-source build unless commercial.                                         |
| Qt Quick Timeline   | qtquicktimeline      | **GPLv3**                              | —                   | Excluded from open-source build unless commercial.                                         |
| Qt Quick 3D Physics | qtquick3dphysics     | **GPLv3**                              | —                   | Excluded from open-source build unless commercial.                                         |
| Qt Canvas Painter   | not installed (TP)   | **GPLv3**                              | —                   | Technology Preview; not for stable public API.                                             |
| Qt Qml Compiler     | not installed        | **GPLv3**                              | —                   | Open-source Qt qmllint/qml compiler tooling; runtime AOT compiler is a commercial feature. |
| Qt HTTP Server      | qthttpserver         | **GPLv3**                              | —                   | Excluded from open-source build unless commercial.                                         |
| Qt Remote Objects   | qtremoteobjects      | LGPLv3 / GPLv2                         | verify              | cross-process proxy.                                                                       |
| Qt SerialBus        | qtserialbus          | LGPLv3 / GPLv2                         | verify              | CAN/Modbus.                                                                                |
| Qt Serial Port      | qtserialport         | LGPLv3 / GPLv2                         | verify              | serial.                                                                                    |
| Qt Positioning      | qtpositioning        | LGPLv3 / GPLv2                         | verify              | GPS/location.                                                                              |
| Qt Sensors          | qtsensors            | LGPLv3 / GPLv2                         | **no QNX per docs** | Tier 3 per Prompt 04; do not use.                                                          |
| Qt TextToSpeech     | qtspeech             | LGPLv3 / GPLv2                         | verify              | needs QNX backend.                                                                         |
| Qt WebSockets       | qtwebsockets         | LGPLv3 / GPLv2                         | verify              | real-time bus.                                                                             |
| Qt Print Support    | in qtbase            | LGPLv3 / GPLv2                         | verify              | printing.                                                                                  |
| Qt PDF              | qtpdf? not in Cellar | commercial / extension                 | —                   | Tier 2; extension module. Verify.                                                          |
| Qt MQTT             | not installed        | **GPLv3**                              | —                   | Excluded from open-source build unless commercial.                                         |
| Qt OPC UA           | not installed        | **GPLv3**                              | —                   | Excluded from open-source build unless commercial.                                         |

### Tier 3 — excluded

| Module                                    | Reason                                                                                          |
| ----------------------------------------- | ----------------------------------------------------------------------------------------------- |
| Qt Quick Layouts                          | Yoga is the layout authority; anchors/Qt layouts conflict.                                      |
| Qt Widgets / Quick Widgets / UI Tools     | C++ widget stack; not used.                                                                     |
| Active Qt                                 | Windows-only.                                                                                   |
| Qt Wayland Compositor                     | not QNX.                                                                                        |
| Qt Sensors                                | Qt docs list Android/iOS/Windows only; use §5 QNX paths instead.                                |
| Qt WebView                                | QNX has no native webview to delegate.                                                          |
| Qt WebEngine                              | Chromium; cross-build for QNX aarch64 out of scope; `react-native-webview` is a documented gap. |
| Qt 3D / Qt Charts / Qt Data Visualization | deprecated upstream.                                                                            |

## Related files

- `docs/packaging.md`
- `docs/adr/0001-qt-quick-backend.md`
- `docs/adr/0010-macos-qt-development-host.md`
- `CMakeLists.txt`
- `docs/cited-seams.md`

## Notes

- The macOS workspace cannot verify per-module `LICENSES/` or `SBOM` files
  because the Homebrew kegs are binary-only. The next step is to acquire the
  pinned Qt LTS source and re-audit every row.
- The `qtbase` keg does contain `FindSlog2.cmake` and `FindPPS.cmake`, which
  confirms QNX-specific symbols live in the Qt source tree and not just in a
  fork. These are recorded in `docs/cited-seams.md`.
- `Qt Quick Controls` is part of `qtdeclarative` since Qt 6.3; the `qt` Homebrew
  keg is a meta-package that pulls it in. The module still has an independent
  name and concern for the component library.

### Observed OpenGL runtime dependency

The 6.12.0 QNX build also produces Qt OpenGL from qtbase, required by the
selected Quick rendering path. It is included in this restricted private
evaluation runtime. `qtbase/src/opengl/qopenglshaderprogram.cpp` offers
`LicenseRef-Qt-Commercial` alongside LGPLv3/GPL alternatives. The built QNX QPA
plugin is `plugins/platforms/libqqnx.so`; QML plugin filenames and the shared
library closure were inspected from the built artifacts, rather than assumed.
