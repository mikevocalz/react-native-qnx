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

## React Native third-party dependency pins

React Native 0.86.2 does not vendor its C++ third-party dependencies, so
`cmake/dependencies/ReactNativeThirdParty.cmake` fetches them at the versions the
installed package itself pins. Versions and flags are read from the package, not
chosen:

| Dependency        | Version       | Pinned by                                          |
| ----------------- | ------------- | -------------------------------------------------- |
| folly             | 2024.11.18.00 | `scripts/cocoapods/helpers.rb:50`                  |
| fmt               | 12.1.0        | `third-party-podspecs/fmt.podspec:11`              |
| glog              | 0.3.5         | `third-party-podspecs/glog.podspec:11`             |
| double-conversion | 1.1.6         | `third-party-podspecs/DoubleConversion.podspec:11` |
| fast_float        | 8.0.0         | `third-party-podspecs/fast_float.podspec:11`       |

Recipes ported rather than invented:

| Element                   | Installed authority                                                |
| ------------------------- | ------------------------------------------------------------------ |
| folly compile definitions | `ReactAndroid/src/main/jni/third-party/folly/CMakeLists.txt:10-20` |
| glog `.h.in` token map    | `@react-native/gradle-plugin` `PrepareGlogTask.kt`                 |
| glog `config.h`           | `ReactAndroid/src/main/jni/third-party/glog/config.h`              |
| double-conversion staging | `ReactAndroid/build.gradle.kts:340-349`                            |

Deviations from React Native's Android recipe, each deliberate:

- **Boost is React Native's trimmed distribution, and its assembly is not
  built.** The 1.4 MB `boost-for-react-native` tarball carries the headers folly
  needs; `folly/json/json.cpp` includes `boost/algorithm/string.hpp`. React
  Native also compiles boost's context-switching assembly for folly's fibers,
  which nothing here uses.
- **`FOLLY_HAVE_XSI_STRERROR_R` and `FOLLY_HAVE_RECVMMSG` are omitted** as
  Bionic-specific and unused respectively. Every other folly definition and all
  29 `folly_runtime` sources are React Native's.
- **glog is compiled with `-w` and `NDEBUG`.** Third-party warnings are not ours
  to fix, and React Native applies its own suppressions rather than compiling
  glog clean.

Note on the debug-string subsystem: an earlier revision defined
`REACT_NATIVE_PRODUCTION` to compile out `RN_DEBUG_STRING_CONVERTIBLE`, because
`ShadowViewMutation.cpp` referenced `getDebugName`/`getDebugProps` from
`ShadowView.cpp`. That flag is gone. `react_renderer_debug` defines
`RN_ENABLE_DEBUG_STRING_CONVERTIBLE` publicly for Debug builds
(`react/renderer/debug/CMakeLists.txt:22-23`), so defining the opposite would
have split the flag across translation units. Building the mounting module
supplies the symbols instead.

React Native sources are no longer compiled one file at a time; the renderer
modules described above are built as React Native itself defines them.

## Fabric mount contract mirrored on the host

`QtQuickMountingLayer` exposes the platform half of the mounting layer. Its
operation set and value types are taken field-by-field from installed React
Native 0.86.2 sources, so the future adapter is a direct translation rather than
a reinterpretation.

| Mirrored element                                                | Installed path and line                                             | Mirror in this repository                   |
| --------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------------------------------- |
| `ShadowViewMutation::Type` (Create/Delete/Insert/Remove/Update) | `ReactCommon/react/renderer/mounting/ShadowViewMutation.h:84`       | `create/destroy/insert/remove/updateLayout` |
| `ShadowViewMutation` fields `type`, `parentTag`, `index`        | `ReactCommon/react/renderer/mounting/ShadowViewMutation.h:94,95,98` | operation parameters                        |
| `Tag`                                                           | `ReactCommon/react/renderer/core/ReactPrimitives.h:17`              | `rnqnx::fabric::Tag`                        |
| `LayoutMetrics::frame`                                          | `ReactCommon/react/renderer/core/LayoutMetrics.h:26`                | `rnqnx::fabric::LayoutFrame`                |
| `Rect{Point origin, Size size}`                                 | `ReactCommon/react/renderer/graphics/Rect.h:23-25`                  | `LayoutFrame` x/y/width/height              |
| `Float`                                                         | `graphics/platform/cxx/react/renderer/graphics/Float.h:18`          | `float` fields in `LayoutFrame`             |

Evidence level: **compiled against the installed headers**, not merely read.
`rnqnx_rn_headers_probe` asserts each mirror with `static_assert`, so a mismatch
between this table and the installed package is a build failure rather than a
stale document. `ShadowViewMutationAdapter` consumes real
`facebook::react::ShadowViewMutation` values built through React Native's own
static factories.

## Installed Qt 6.11.1 (macOS development host)

Homebrew Qt 6.11.1 at `/opt/homebrew/opt/qt`, used per ADR-0010. Framework
layout, so headers live under `lib/<Module>.framework/Headers`. These are public
headers only; no private Qt header is included. Every symbol here must be
re-audited against the Qt LTS tag that G0-B pins for the target.

| Symbol                                                   | Installed path and line                                      | Used by                            | Remaining gate                   |
| -------------------------------------------------------- | ------------------------------------------------------------ | ---------------------------------- | -------------------------------- |
| `QQmlComponent(QQmlEngine*, QObject*)`                   | `lib/QtQml.framework/Headers/qqmlcomponent.h:46`             | `QtQuickItemFactory::componentFor` | Presence in the pinned Qt LTS    |
| `QQmlComponent::setData(const QByteArray&, const QUrl&)` | `lib/QtQml.framework/Headers/qqmlcomponent.h:94`             | inline type-source compilation     | Presence in the pinned Qt LTS    |
| `QQmlComponent::isError`, `errorString`                  | `lib/QtQml.framework/Headers/qqmlcomponent.h:63,69`          | compile/instantiate failure path   | Presence in the pinned Qt LTS    |
| `QQmlComponent::create(QQmlContext*)`                    | `lib/QtQml.framework/Headers/qqmlcomponent.h:75`             | item instantiation                 | Ownership behavior on the target |
| `QQuickItem::setParentItem(QQuickItem*)`                 | `lib/QtQuick.framework/Headers/qquickitem.h:186`             | mounting/parenting                 | Target scene-graph behavior      |
| `QQuickItem::setX`, `setWidth`, `setSize`, `setVisible`  | `lib/QtQuick.framework/Headers/qquickitem.h:207,214,228,247` | Yoga metrics application           | Target layout/paint behavior     |
| `QQuickWindow::contentItem()`                            | `lib/QtQuick.framework/Headers/qquickwindow.h:97`            | root item host                     | QNX QPA window behavior          |
| `QQuickWindow::setColor(const QColor&)`                  | `lib/QtQuick.framework/Headers/qquickwindow.h:127`           | root background                    | QNX QPA window behavior          |
| Offscreen QPA plugin                                     | `share/qt/plugins/platforms/libqoffscreen.dylib`             | headless CTest run                 | Not applicable to the target     |

Evidence level: host-compiled and host-executed under strict warnings. This is
not Qt-on-QNX evidence; the QNX QPA plugin over Screen remains unbuilt and
unproven, and no Qt code has run on the Pi.

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
