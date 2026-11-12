# Architecture Decision Records

This directory records significant architectural decisions for `react-native-qnx`. Each ADR follows the template in `adr-template.md`.

## Index

| ADR                                                  | Title                                                           | Status                                             | Date       |
| ---------------------------------------------------- | --------------------------------------------------------------- | -------------------------------------------------- | ---------- |
| [0001](0001-qt-quick-backend.md)                     | Qt Quick as the QNX component backend                           | Accepted; scoped Pi 400 proof                      | 2026-08-13 |
| [0002](0002-qt-quick-item-instantiation.md)          | Qt Quick built-in-item instantiation                            | Provisional; Pi benchmark open                     | 2026-08-13 |
| [0003](0003-skia-initial-renderer.md)                | Skia as the initial graphics renderer                           | Superseded                                         | 2026-07-19 |
| [0004](0004-nitro-modules.md)                        | Nitro Modules for native application APIs                       | Accepted                                           | 2026-07-19 |
| [0005](0005-hermes-only.md)                          | Hermes as the initial JavaScript engine                         | Accepted                                           | 2026-07-19 |
| [0006](0006-expo-dev-builds.md)                      | Expo development builds instead of Expo Go                      | Accepted                                           | 2026-07-19 |
| [0007](0007-self-hosted-build-infra.md)              | Self-hosted QNX build infrastructure                            | Accepted                                           | 2026-07-19 |
| [0008](0008-version-pinning.md)                      | Version-pinning strategy                                        | Accepted                                           | 2026-07-19 |
| [0009](0009-fabric-only.md)                          | Fabric-only renderer architecture                               | Accepted                                           | 2026-08-13 |
| [0010](0010-macos-qt-development-host.md)            | macOS as a development host                                     | Accepted                                           | 2026-08-14 |
| [0011](0011-qt-module-licensing.md)                  | Qt module licensing gate                                        | Restricted evaluation; open-source matrix proposed | 2026-08-14 |
| [0012](0012-quick-controls-styling.md)               | Quick Controls style for QNX                                    | Proposed                                           | 2026-08-14 |
| [0013](0013-open-source-qt.md)                       | Open-source Qt without a paid-license prerequisite              | Accepted                                           | 2026-11-07 |
| [0015](0015-marathon-linux-wayland-native-target.md) | MarathonOS Linux/Wayland target on the shared Qt/Fabric backend | Proposed                                           | 2026-11-02 |
| [0016](0016-window-chrome-on-qnx-screen.md)          | Client-drawn window chrome on QNX Screen                        | Proposed; host tests only                          | 2026-11-12 |
| [0017](0017-qt-qnx-optimization-level.md)            | Compile public-source Qt for QNX at -O2                         | Accepted; rebuild pending                          | 2026-11-12 |
| [0018](0018-runner-hosting-and-sdp-boundary.md)      | Runner hosting, signing and the QNX SDP boundary                | Accepted; no runner published                      | 2026-11-12 |

ADR-0015 was filed as a second ADR-0013 and renumbered on 2026-11-11. Links to
`0013-marathon-linux-wayland-native-target.md` now point to
[ADR-0015](0015-marathon-linux-wayland-native-target.md); ADR-0013 means the
open-source Qt decision only.
