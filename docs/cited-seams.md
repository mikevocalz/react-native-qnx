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

## React Native renderer modules built from source

`cmake/dependencies/ReactNativeRenderer.cmake` builds 41 React Native modules by
adding React Native's own per-module `CMakeLists.txt` files rather than
rewriting them, so source lists, compile flags, and link lists stay owned by
React Native.

| Element                        | Installed authority                                                               |
| ------------------------------ | --------------------------------------------------------------------------------- |
| Platform selection             | `ReactCommon/cmake-utils/internal/react-native-platform-selector.cmake:17-24`     |
| React Native compile flags     | `ReactCommon/cmake-utils/react-native-flags.cmake`                                |
| `target_merge_so` (no-op here) | `ReactAndroid/src/main/jni/first-party/jni-lib-merge/SoMerging-utils.cmake:17-19` |
| Yoga target name               | `ReactCommon/yoga/yoga/CMakeLists.txt` defines `yogacore`, aliased to `yoga`      |

Host-specific handling, none of it a change to React Native's recipes:

- With `ANDROID` unset, React Native's selector picks each module's generic
  `cxx` implementation — the same path QNX will take.
- Nine `platform/cxx` directories go on the global include path, matching React
  Native's Android assembler, because modules such as
  `react_renderer_componentregistry` reach `HostPlatformViewProps.h` without
  linking `rrc_view`.
- Most modules are CMake OBJECT libraries, whose objects are not archived into a
  static library that links them, so the aggregate target attaches
  `$<TARGET_OBJECTS:...>` for each.
- `glog_init` is React Native's Android glog bootstrap and resolves to an empty
  interface target.

Evidence level: host-compiled and host-linked. None of these modules has been
cross-compiled with `qcc` or run on the Pi 4.

## Hermes

React Native 0.86.2 carries two Hermes versions and the newer one is the
default. Every build path opts _out_ of it rather than into it:

| Fact                                              | Installed authority                                         |
| ------------------------------------------------- | ----------------------------------------------------------- |
| `HERMES_VERSION_NAME=0.17.0` (legacy)             | `sdks/hermes-engine/version.properties:1`                   |
| `HERMES_V1_VERSION_NAME=250829098.0.16` (default) | `sdks/hermes-engine/version.properties:2`                   |
| Apple: v1 unless `RCT_HERMES_V1_ENABLED == "0"`   | `sdks/hermes-engine/hermes-utils.rb:89-91`                  |
| Android: `convention(true)`                       | `@react-native/gradle-plugin` `PrivateReactExtension.kt:62` |
| `hermes-compiler` pinned to `250829098.0.16`      | `node_modules/react-native/package.json:179`                |
| Source URL template                               | `ReactAndroid/hermes-engine/build.gradle.kts:106-111`       |
| Git source form                                   | `sdks/hermes-engine/hermes-utils.rb:10,176-185`             |

So `hermes-v250829098.0.16` is React Native 0.86.2's Hermes. Nothing usable
ships in the package: `sdks/hermes` and `sdks/hermesc` are both absent, and
`ReactCommon/hermes` holds only React Native's glue, which includes
`<hermes/...>` and needs the tree present.

The tag is pinned to commit `90f23852efcd361315688e2904d2446707fa274c` and the
build fails if it ever resolves elsewhere. A tarball SHA256 would have been the
obvious pin and is the wrong one: GitHub's generated archives are not
byte-reproducible, so the hash would eventually fail for reasons unrelated to
the source changing.

Options set explicitly, because this tree's defaults differ from 0.17.0's:

| Option                         | Set   | Reason                                                                      |
| ------------------------------ | ----- | --------------------------------------------------------------------------- |
| `HERMES_ENABLE_INTL`           | `OFF` | React Native forces it on where ICU ships; QNX has no ICU story yet         |
| `HERMES_ENABLE_DEBUGGER`       | `ON`  | defaults OFF in this tree, unlike 0.17.0                                    |
| `HERMES_BUILD_APPLE_FRAMEWORK` | `OFF` | defaults OFF here; we want a plain dylib                                    |
| `HERMES_ENABLE_TEST_SUITE`     | `OFF` | upstream's own suite is not our gate                                        |
| `JSI_DIR`                      | RN's  | one JSI in the process, matching what React Native passes on both platforms |

Linkage is chosen by target name — `lib/CMakeLists.txt` declares `hermesvm`
(SHARED) and `hermesvm_a` (STATIC); there is no `BUILD_SHARED_LIBS` switch.
`IMPORT_HOST_COMPILERS` is left unset: it exists for cross-compiling, where a
host `hermesc` must be built first
(`lib/InternalJavaScript/CMakeLists.txt:6-14`). The QNX cross-build will need
that two-stage bootstrap.

One trap worth recording: React Native ships two CMakeLists for JSI and they are
not equivalent. `ReactCommon/jsi/CMakeLists.txt:17` globs `jsi/*.cpp` and so
includes `JSIDynamic.cpp`; `ReactCommon/jsi/jsi/CMakeLists.txt:9` builds
`jsi.cpp` alone. Hermes adds the inner one, so when Hermes provides `jsi` the
`folly::dynamic` bridge is missing and `AppRegistryBinding` fails to link. That
single translation unit is added back explicitly.

## What a platform must implement itself

Audited against the installed package, then implemented. Everything from
`Scheduler` downward is generic C++ and used as-is; these are the pieces that
had no generic implementation:

| Piece                                         | Interface                                                      | Implemented as                                 |
| --------------------------------------------- | -------------------------------------------------------------- | ---------------------------------------------- |
| `EventBeat` subclass                          | `react/renderer/core/EventBeat.h:62`, `request()` :90          | `QtEventBeat`, ticking off the Qt loop         |
| `ComponentRegistryFactory`                    | `componentregistry/ComponentDescriptorFactory.h:25-27`         | `makeComponentRegistryFactory()`               |
| `SchedulerDelegate` subclass                  | `renderer/scheduler/SchedulerDelegate.h:21`, 13 pure virtuals  | `QtSchedulerDelegate`                          |
| `bindHasComponentProvider`                    | `componentregistry/native/NativeComponentRegistryBinding.h:26` | not yet — needed once JS view configs run      |
| `MessageQueueThread`, `PlatformTimerRegistry` | required by `ReactInstance`                                    | avoided — the host drives the runtime directly |

`getDefaultComponentRegistryFactory()` is declared at
`ComponentDescriptorFactory.h:29` and defined nowhere in the package, which is
why every platform writes its own.

Four behaviours found by running this, all React Native's:

- **The fallback descriptor must be `UnimplementedNativeView`, not
  `UnimplementedView`.** The latter's `getComponentName()` dereferences
  `flavor_` as a `std::string`
  (`UnimplementedViewComponentDescriptor.cpp:17-19`), so registering it with a
  null flavor segfaults inside `setFallbackComponentDescriptor`, which calls
  `registerComponentDescriptor` immediately
  (`ComponentDescriptorRegistry.cpp:143-147`). React Native's own comment names
  `UnimplementedNativeViewComponentDescriptor` as the default
  (`ComponentDescriptorRegistry.cpp:97-99`), and Android installs exactly that.
- **`createComponentDescriptorRegistry` returns a pointer-to-const**, so the
  fallback goes in through a `const_cast`. That is React Native's own pattern
  (`DefaultComponentsRegistry.cpp:54-56`), not a workaround.
- **Stopping any surface calls into JavaScript.**
  `UIManager::stopSurface` runs `AppRegistryBinding::stopSurface`
  unconditionally (`UIManager.cpp:292-294`), which throws unless
  `global.RN$stopSurface` exists. An empty surface has no React app to tear
  down, so the host installs a no-op only when the global is absent; a real
  bundle's implementation (`Libraries/Renderer/shims/ReactFabric.js:27`) always
  wins.
- **`react_codegen_rncore` is required by three renderer modules** (scheduler,
  animationbackend, viewtransition) and is codegen output with no CMakeLists.
  `ReactCommon/react/renderer/components/rncore/` is five headers forwarding to
  `FBReactNativeSpec`, which does not exist under `ReactCommon` at all. React
  Native does ship a pre-generated copy under `React/FBReactNativeSpec`, and its
  renderer half is plain C++ despite living in the Apple tree, so those five
  sources are built as `react_codegen_rncore`. Known ceiling: these are
  Apple-flavoured generation, and anything past the core component set needs
  React Native's codegen run properly.

## Bundle-time and bridgeless globals

Metro resolves platform files by extension and its default platform list has no
`qnx`, so `@rn-qnx/metro-config-qnx` appends it. Without that, a `Foo.qnx.js`
file is never preferred over `Foo.js` — the entire mechanism an out-of-tree
platform depends on.

React Native's JavaScript branches on several globals before a bundle can even
finish evaluating. The host installs them before any evaluation:

| Global                      | Why it is required                                                         |
| --------------------------- | -------------------------------------------------------------------------- |
| `RN$Bridgeless`             | `Libraries/Core/setUpBatchedBridge.js:16`, `NativeComponentRegistry.js:57` |
| `__fbBatchedBridgeConfig`   | `Libraries/BatchedBridge/NativeModules.js:185-192` throws without it       |
| `RN$registerCallableModule` | `Libraries/Core/registerCallableModule.js:20` in bridgeless mode           |

`__fbBatchedBridgeConfig` is installed with an empty `remoteModuleConfig`, which
is the truthful answer rather than a stub: this platform has no legacy bridge
modules, and TurboModules are the mechanism. `RN$registerCallableModule` mirrors
`ReactInstance.cpp:586-606` and keeps its registrations; what is missing is a
native caller, not the registry.

The bundle's own globals come from React Native's JavaScript:
`global.RN$AppRegistry` at `Libraries/ReactNative/AppRegistry.js:30`, which
`AppRegistryBinding::startSurface` requires, and `global.RN$stopSurface` at
`Libraries/Renderer/shims/ReactFabric.js:27`.

**Evaluating the bundle stops at TurboModules.** The playground bundle calls
`TurboModuleRegistry.getEnforcing` for nine modules — DeviceInfo,
ExceptionsManager, FileReaderModule, NativeIdleCallbacksCxx,
NativeMicrotasksCxx, SegmentFetcher, SourceCode, UIManager, WebSocketModule —
and the first one throws. That is the next slice, not a defect in what exists.

## The JavaScript binding surface

`nativeFabricUIManager` is what React's renderer drives, and driving it directly
proves the JavaScript-to-mount path without React or a bundle in the way:

| JS method          | Installed authority            | Arguments                                            |
| ------------------ | ------------------------------ | ---------------------------------------------------- |
| `createNode`       | `UIManagerBinding.cpp:219-252` | tag, componentName, surfaceId, props, instanceHandle |
| `createChildSet`   | `UIManagerBinding.cpp:424-438` | none                                                 |
| `appendChildToSet` | `UIManagerBinding.cpp:441-460` | childSet, node                                       |
| `completeRoot`     | `UIManagerBinding.cpp:462-…`   | surfaceId, childSet                                  |

`instanceHandle` must be an object; React Native asserts on a null one
(`UIManagerBinding.cpp:234-238`).

The component predicate JavaScript uses to ask whether a native component
exists is `bindHasComponentProvider`
(`componentregistry/native/NativeComponentRegistryBinding.h:26`, built from the
`rrc_native` module). Names arrive as React view names, so they are normalised
through `componentNameByReactViewName` first — that function strips an `RCT`
prefix and rewrites `Text`, `VirtualText`, and `ImageView`.

`ComponentDescriptorRegistry` offers no name-based existence check, and with a
fallback installed `at()` never throws, so it cannot answer the predicate. The
answer comes from `registeredComponentNames()`, the same list the registry
factory registers from, so the two cannot drift apart.

## Shadow-tree pipeline seams

The end-to-end host test drives React Native's own machinery rather than
hand-built mutation lists:

| Seam                                      | Installed path and symbol                                           |
| ----------------------------------------- | ------------------------------------------------------------------- |
| `ShadowTree` construction                 | `react/renderer/mounting/ShadowTree.h:88-93`                        |
| `ShadowTree::commit`                      | `react/renderer/mounting/ShadowTree.h:120`                          |
| `ShadowTreeCommitTransaction`             | `react/renderer/mounting/ShadowTree.h:26`                           |
| `ShadowTreeDelegate` (four pure virtuals) | `react/renderer/mounting/ShadowTreeDelegate.h`                      |
| `MountingCoordinator::pullTransaction`    | `react/renderer/mounting/MountingCoordinator.h:64`                  |
| `ShadowNodeFragment::children`            | `react/renderer/core/ShadowNodeFragment.h:27`                       |
| Element tree builder                      | `react/renderer/element/{ComponentBuilder,Element}.h`               |
| Yoga style setters                        | `yoga/style/Style.h:108,136,178,199`; `yoga/style/StyleLength.h:32` |
| `BaseViewProps::collapsable`              | `react/renderer/components/view/BaseViewProps.h:107`                |

Two behaviours found by running it, both React Native's and neither worked
around:

- **View flattening is real and silent.** A `<View>` carrying only layout props
  produces no mount mutation at all; React Native folds its layout into the
  parent. The first run emitted exactly one mutation — an Update of the root,
  sized to the child — and no Create. The views under test set
  `collapsable = false` so the layout assertions have a mounted item to assert
  against. A platform gets flattening for free, but anyone debugging a missing
  view needs to know this first.
- **`react/renderer/element/testUtils.h` is not usable here.** Its
  `simpleComponentBuilder` registers modal and text descriptors, which include
  codegen output (`FBReactNativeSpec`) this repository does not generate. The
  test registers `RootComponentDescriptor` and `ViewComponentDescriptor`
  directly instead of pretending those artifacts exist.

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
