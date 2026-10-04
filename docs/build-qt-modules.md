# Qt Module Build Discipline

This document is the source of truth for which Qt modules may be linked into the
`react-native-qnx` build. Adding a module is a one-line, reviewable change to
this file and to `CMakeLists.txt` (or the QNX equivalent). The owner of that
change must also record the resulting size and cross-build time on the target.

## Rule

1. Every module in this list has a recorded tier, justification, and license
   status from ADR-0011.
2. A module may not be added if its open-source license is incompatible with
   the project posture.
3. The host macOS build and the QNX target build must use the same source tag
   for any module, but they may include different subsets. The macOS subset is
   `RNQNX_QT_HOST_MODULES` in `CMakeLists.txt`; the QNX subset is recorded in
   the per-target build documentation once a licensed SDP exists.
4. Each addition must record the QNX binary-size and cross-build-time delta
   before the PR is accepted. Cells marked **TBD** are blockers for the
   corresponding module.

## Host build module list (Qt 6.11.1)

`CMakeLists.txt` declares:

```cmake
set(RNQNX_QT_HOST_MODULES Core Gui Qml Quick)
find_package(Qt6 6.11.1 COMPONENTS ${RNQNX_QT_HOST_MODULES} QUIET)
```

The view target links `Qt6::Quick`, `Qt6::Qml`, and `Qt6::Gui` explicitly. These
are the minimum modules required for `QtQuickItemFactory`, `QnxImageProvider`,
and `QtQuickMountingLayer`.

| CMake component | Qt module | Tier | Justification                              | License status (open-source) | Host installed | QNX size delta | QNX build time delta |
| --------------- | --------- | ---- | ------------------------------------------ | ---------------------------- | -------------- | -------------- | -------------------- |
| `Core`          | Qt Core   | 0    | event loop, `QMetaObject`, `QObject` model | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `Gui`           | Qt GUI    | 0    | text/font, `QImage`, platform abstractions | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `Qml`           | Qt Qml    | 0    | `QQmlComponent` type compilation           | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `Quick`         | Qt Quick  | 0    | `QQuickItem`, `QQuickWindow`, scene graph  | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |

## Target build candidate modules

These modules are the ones from ADR-0011 that are **not** currently in the host
build but are candidates for the QNX target as features land. They must be
added one at a time with size/time data.

### Tier 1 component-backing

| CMake component       | Qt module                    | Tier | Justification                                    | License status (open-source) | Host installed | QNX size delta | QNX build time delta |
| --------------------- | ---------------------------- | ---- | ------------------------------------------------ | ---------------------------- | -------------- | -------------- | -------------------- |
| `QuickControls2`      | Qt Quick Controls            | 1    | native `Button`, `Switch`, `Slider`, `TextField` | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `QuickControls2Basic` | Basic style (Quick Controls) | 1    | default style per ADR-0012                       | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `QuickEffects`        | Qt Quick Effects             | 1    | `filter`, blur, drop-shadow                      | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `Svg`                 | Qt SVG                       | 1    | SVG icon/image files                             | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `Multimedia`          | Qt Multimedia                | 1    | `<Video>`, audio, camera                         | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `QuickDialogs2`       | Qt Quick Dialogs             | 1    | file/color pickers                               | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `ImageFormats`        | Qt Image Formats             | 1    | extra image codecs                               | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `Sql`                 | Qt SQL                       | 1    | local SQL (`expo-sqlite`)                        | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |

### Tier 2 evaluate per need

| CMake component          | Qt module           | Tier | Justification                             | License status (open-source) | Host installed | QNX size delta | QNX build time delta |
| ------------------------ | ------------------- | ---- | ----------------------------------------- | ---------------------------- | -------------- | -------------- | -------------------- |
| `QuickControls2Material` | Material style      | 2    | opt-in theme                              | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `QuickControls2Fusion`   | Fusion style        | 2    | opt-in theme                              | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `Quick3D`                | Qt Quick 3D         | 2    | 3D scenes                                 | **GPLv3**                    | yes            | —              | —                    |
| `Graphs`                 | Qt Graphs           | 2    | charts                                    | **GPLv3**                    | yes            | —              | —                    |
| `Lottie`                 | Qt Lottie Animation | 2    | Lottie JSON                               | **GPLv3**                    | yes            | —              | —                    |
| `QuickTimeline`          | Qt Quick Timeline   | 2    | keyframe native animation                 | **GPLv3**                    | yes            | —              | —                    |
| `Quick3DPhysics`         | Qt Quick 3D Physics | 2    | 3D physics                                | **GPLv3**                    | yes            | —              | —                    |
| `CanvasPainter`          | Qt Canvas Painter   | 2    | canvas 2D (TP)                            | **GPLv3**                    | yes            | —              | —                    |
| `HttpServer`             | Qt HTTP Server      | 2    | local HTTP                                | **GPLv3**                    | yes            | —              | —                    |
| `QmlCompiler`            | Qt Qml Compiler     | 2    | AOT compile (open-source tools are GPLv3) | **GPLv3**                    | not installed  | —              | —                    |
| `RemoteObjects`          | Qt Remote Objects   | 2    | cross-process proxy                       | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `SerialBus`              | Qt SerialBus        | 2    | CAN/Modbus                                | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `SerialPort`             | Qt Serial Port      | 2    | serial I/O                                | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `Positioning`            | Qt Positioning      | 2    | GPS/location                              | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `TextToSpeech`           | Qt TextToSpeech     | 2    | TTS                                       | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `WebSockets`             | Qt WebSockets       | 2    | real-time sockets                         | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `PrintSupport`           | Qt Print Support    | 2    | printing                                  | LGPLv3 / GPLv2               | yes            | TBD            | TBD                  |
| `Pdf`                    | Qt PDF              | 2    | PDF viewing                               | extension / commercial       | yes            | TBD            | TBD                  |

## Excluded modules

See ADR-0011 for the full list and the reasons. The most important ones are
`QtQuickLayouts` (conflicts with Yoga), `QtWebEngine` and `QtWebView` (no QNX
backend or Chromium cross-build scope), and `QtVirtualKeyboard` (GPLv3-only
open-source; product decision required).

## Measurements

QNX size and build time columns are filled by running the QNX cross-build
before/after a module is added. The `scripts/validate-g0-a.sh` flow (when a
licensed SDP is available) is the entry point for these numbers.

## Opt-in host WebSocket experiment (not a target module addition)

`RNQNX_ENABLE_WEBSOCKETS=ON` requires `Core`, `Network`, and `WebSockets` at Qt
6.11.1 for the experimental host transport. It defaults to **OFF** and is rejected
when `CMAKE_SYSTEM_NAME=QNX`. This does not change `RNQNX_QT_HOST_MODULES`, the
baseline renderer dependencies, or the accepted QNX subset.

Network provides the asynchronous TCP/TLS substrate; Tier 2 WebSockets provides
the RFC 6455 handshake, framing, and lifecycle for RN's WebSocket API. Both are
LGPLv3/GPLv2 open-source modules under the ADR-0011 posture. Their optional host
use is justified by real loopback and TurboModule contract tests. Target
binary-size and cross-build-time deltas, including transitive TLS costs, are
still **TBD** and remain acceptance blockers. See
[host WebSocket readiness](websocket-host-readiness.md) for reproducible tests,
limitations, and the evidence required before removing the target guard.
