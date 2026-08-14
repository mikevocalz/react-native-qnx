# Cited Seams Ledger

Every native implementation entry records both its authority and its evidence
level. “Public-doc verified” does not mean “installed-header verified” or
“target verified.”

## G0 toolchain

| Seam                 | Authority                                                                                                                                       | Evidence            | Remaining gate                       |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- | ------------------- | ------------------------------------ |
| QNX CMake target     | QNX Everywhere Porting Guide, `CMAKE_SYSTEM_NAME`, `CMAKE_SYSTEM_PROCESSOR`, `CMAKE_CXX_COMPILER`, `CMAKE_CXX_COMPILER_TARGET`, `CMAKE_SYSROOT` | Public-doc verified | Configure with installed SDP         |
| AArch64le qcc target | QNX SDP 8 `qcc` utility reference, `gcc_ntoaarch64le` and `qcc -V`                                                                              | Public-doc verified | Commit safe installed `qcc -V` lines |
| C++20 runtime proof  | `vnext/probes/cxx20/main.cpp`: standard concepts, ranges, designated initialization, `std::jthread`                                             | Host-compiled       | qcc cross-build and Pi execution     |

Sources:

- [QNX Everywhere Porting Guide — CMake](https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.qpg/topic/port_CMake.html)
- [QNX SDP 8 Utilities Reference — q++, qcc](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/q/qcc.html)

## Current QNX Screen probe

The production probe calls the following QNX Screen seams from
`native/platform/qnx/screen/QnxScreenSurface.cpp`:

| Symbols                                                                                                                                                                   | Authority                              | Evidence                                          | Remaining gate                                     |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------- | ------------------------------------------------- | -------------------------------------------------- |
| `screen_create_context`, `screen_destroy_context`                                                                                                                         | QNX Screen 8 function reference        | Public-doc verified; host fake exercised          | Installed `screen/screen.h`, qcc, target lifecycle |
| `screen_create_window_type`, `screen_destroy_window`                                                                                                                      | QNX Screen 8 function reference        | Public-doc verified; host fake exercised          | Installed header and target window manager         |
| `screen_set_window_property_iv`, `SCREEN_PROPERTY_FORMAT`, `SCREEN_PROPERTY_USAGE`, `SCREEN_PROPERTY_BUFFER_SIZE`, `SCREEN_PROPERTY_SIZE`                                 | QNX Screen 8 property reference        | Public-doc verified; exact host observations      | Installed constants and BSP behavior               |
| `screen_create_window_buffers`, `screen_get_window_property_iv`, `screen_get_window_property_pv`, `SCREEN_PROPERTY_RENDER_BUFFER_COUNT`, `SCREEN_PROPERTY_RENDER_BUFFERS` | QNX Screen 8 buffer/property reference | Public-doc verified; failure/ownership host tests | Installed ABI and compositor behavior              |
| `screen_fill`, `SCREEN_BLIT_COLOR`, destination tokens, `SCREEN_BLIT_END`                                                                                                 | QNX Screen 8 fill/blit reference       | Public-doc verified; exact ARGB host test         | Driver fill behavior on the Pi                     |
| `screen_post_window`                                                                                                                                                      | QNX Screen 8 post reference            | Public-doc verified; buffer-rotation host test    | Visible presentation and cleanup on the Pi         |

Installed-header reconciliation is queued in
`docs/handoff/QNX_VALIDATION_QUEUE.md`. The fake header is never listed as QNX
authority.

## Installed React Native 0.86.2

| Seam                           | Installed path and symbol                                                                                | Evidence boundary                                          |
| ------------------------------ | -------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| Scheduler host injection       | `node_modules/react-native/ReactCommon/react/renderer/scheduler/SchedulerToolbox.h` — `SchedulerToolbox` | Installed-source verified; no QNX host exists              |
| Mount completion callback      | `.../scheduler/SchedulerDelegate.h` — `schedulerDidFinishTransaction`                                    | Installed-source verified; no QNX delegate exists          |
| Transaction acquisition        | `.../mounting/MountingCoordinator.h` — `pullTransaction`                                                 | Installed-source verified; no QNX mutation executor exists |
| JS Fabric binding installation | `.../uimanager/UIManagerBinding.h` — `createAndInstallIfNeeded`                                          | Installed-source verified; no QNX runtime install exists   |
| Hermes runtime factory         | `node_modules/react-native/ReactCommon/react/runtime/hermes/HermesInstance.h` — `createJSRuntime`        | Installed-source verified; Hermes is not built for QNX     |
| Yoga layout entry              | `node_modules/react-native/ReactCommon/yoga/yoga/YGNode.h` — `YGNodeCalculateLayout`                     | Installed-source verified; bundled RN snapshot             |

## Installed Nitro 0.36.5

| Seam            | Installed path and symbol                                                                                           | Evidence boundary                                                 |
| --------------- | ------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------- |
| Runtime install | `node_modules/react-native-nitro-modules/cpp/entrypoint/InstallNitro.hpp` — `margelo::nitro::install`               | Generic C++ source exists; QNX dispatcher/threading port absent   |
| Hybrid registry | `node_modules/react-native-nitro-modules/cpp/registry/HybridObjectRegistry.hpp` — `registerHybridObjectConstructor` | Registry source exists; no QNX generated spec/registration exists |

Nitrogen's installed platform selection supports iOS/Android/shared only. No
HybridObject implementation may begin before a QNX generation strategy and
committed generated output exist.

## Qt and Expo boundaries

- No Qt source tag is pinned or installed. `QQuickItem`, `QQuickWindow`,
  `QQmlComponent`, QPA implementation paths, plugin filenames, and private/public
  boundaries are **not yet authorized implementation seams**.
- Installed Expo Modules Core has Apple/Android runtime installers only; no QNX
  `globalThis.expo` installer or registry exists.
- Installed Expo Metro defaults omit `qnx`; adding a resolver platform is a
  future source-backed design, not current functionality.
