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

## Window manager and window chrome Screen seams (2026-11-12)

`tools/qnx-winmgr/rnqnx-winmgr.c`, `native/window-chrome/WindowHost.cpp` and
`scripts/qt-public/patches/qtbase-qnx-resize-swap.patch` use these Screen
seams. Authority is the installed SDP 8.0.5 header
`$QNX_TARGET/usr/include/screen/screen.h` (line numbers from that file); the
evidence column says what the Pi 400 showed.

| Symbols                                                                                                                                                                                                                                             | Installed header                                                                                | Evidence                                                                                                                                                                                                                                                                                  | Remaining gate                     |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| `SCREEN_WINDOW_MANAGER_CONTEXT`                                                                                                                                                                                                                     | `screen.h:489`                                                                                  | Pi: context created; app windows report `window manager = ctx-1/<pid>(rnqnx-winmgr)`                                                                                                                                                                                                      | None for this board                |
| `SCREEN_EVENT_CREATE`, `SCREEN_EVENT_POST`, `SCREEN_EVENT_CLOSE`, `SCREEN_EVENT_PROPERTY`, `SCREEN_PROPERTY_OBJECT_TYPE`, `SCREEN_PROPERTY_NAME`, `screen_get_event`                                                                                | `screen.h:4545`, `4600`, `4569`, `4548`, `2412`, `1173`                                         | Pi: manager log lists create, post and close for launcher and app windows                                                                                                                                                                                                                 | Other BSPs                         |
| `SCREEN_PROPERTY_SELF_LAYOUT`                                                                                                                                                                                                                       | `screen.h:2470` (owner may set POSITION, SIZE, VISIBLE, ZORDER only when the manager grants it) | Pi: without it the app's SIZE and POSITION were dropped (window stayed at 0,0, frame stretched); with it the window opens centred and resizes                                                                                                                                             | None for this board                |
| `SCREEN_PROPERTY_MANAGER_STRING`                                                                                                                                                                                                                    | `screen.h:3190`                                                                                 | Pi: a value the manager writes is read back by the app within 10 ms; a value the app writes is not visible to the manager, so it is used only manager to app                                                                                                                              | Behaviour of other Screen versions |
| `SCREEN_EVENT_MANAGER`, `SCREEN_PROPERTY_USER_DATA`, `SCREEN_PROPERTY_WINDOW`, `screen_inject_event`                                                                                                                                                | `screen.h:4657`, `1559`, `1624`                                                                 | Pi: `demolauncher` uses the same message path (disassembly); a `qnxuser` app's injected `rnqnx=minimize` reached the manager. The payload is read with the length in the event's `SCREEN_PROPERTY_SIZE`; a longer read returns nothing                                                    | None for this board                |
| `SCREEN_PROPERTY_FOCUS` (display), `SCREEN_PROPERTY_ZORDER`, `SCREEN_PROPERTY_VISIBLE`                                                                                                                                                              | `screen.h:2165`                                                                                 | Pi: display focus events arrive when a window is clicked; stacking and minimize observed in screenshots                                                                                                                                                                                   | None for this board                |
| `SCREEN_EVENT_POINTER`, `SCREEN_PROPERTY_POSITION` on a manager-owned window                                                                                                                                                                        | `screen.h:4586`, `1259`                                                                         | Pi: for the dock window POSITION carried display coordinates and SOURCE_POSITION window coordinates                                                                                                                                                                                       | Re-check on other Screen versions  |
| `SCREEN_PROPERTY_SIZE`, `SCREEN_PROPERTY_SOURCE_SIZE`, `SCREEN_PROPERTY_POSITION` deferred to the post                                                                                                                                              | `screen.h:1384`, `1259`                                                                         | Pi: setting them just before `eglSwapBuffers` (which posts) applied them with that frame; no stretched frame in a mid-drag screenshot; right edge constant through a left edge drag in the manager's `-v` log                                                                             | None for this board                |
| `SCREEN_INPUT_PROVIDER_CONTEXT`, `screen_create_session_type` (keyboard), `SCREEN_PROPERTY_DEVICES`, `SCREEN_PROPERTY_SESSION` on a device, `SCREEN_EVENT_KEYBOARD`, `SCREEN_PROPERTY_KEY_CAP`, `SCREEN_PROPERTY_MODIFIERS`, `SCREEN_FLAG_KEY_DOWN` | `screen.h:499`, `8914`, `2426`, `2985`, `4593`, `1058`, `1114`, `4694`                          | Pi: with every keyboard routed to the manager's session, injected Alt+Tab opened the switcher; other keys, injected again without a device (the input-provider flag allows it), reached the focused app's TextInput; the Alt release goes to the app that saw the press before the switch | Real USB keyboard on the board     |
| `SCREEN_PROPERTY_SENSITIVITY` with `SCREEN_SENSITIVITY_MASK_NO_FOCUS` on the dock and switcher                                                                                                                                                      | `screen.h:1697`, `4308`                                                                         | Pi: without it the shown switcher took display focus; with it focus stayed on the app and clicks still reached the manager                                                                                                                                                                | None for this board                |
| `SCREEN_EVENT_MANAGER` with `SCREEN_PROPERTY_SUBTYPE` `SCREEN_EVENT_CLOSE`, `screen_send_event`                                                                                                                                                     | `screen.h:4657`, `3200`                                                                         | Pi: the Components app (Qt) closed its window and exited; a test window that ignored it got `SIGTERM` after 3 s                                                                                                                                                                           | None for this board                |

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

## JavaScript platform overrides

React Native resolves twelve files in `Libraries/` by platform extension. Two of
them are load-bearing for any platform, and neither has a working generic
fallback:

| React Native file                             | Symptom when missing                                                                           |
| --------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| `Libraries/Utilities/Platform.js`             | re-exports `./Platform`, resolves to itself, `Platform` is undefined, `Platform.select` throws |
| `Libraries/NativeComponent/BaseViewConfig.js` | resolves to nothing, React reads `bubblingEventTypes` of undefined                             |

`@rn-qnx/react-native-qnx` supplies both under a `Libraries/` tree mirroring
React Native's layout, and `metro-config-qnx` installs a `resolveRequest` that
runs Metro's own resolver first and then swaps the result when an override
exists at the same relative path. Overriding by resolved path rather than by
module name is what makes relative imports such as `./Platform` work, and the
swap only applies to `platform === 'qnx'`.

`Platform.constants` comes from `globalThis.__RNQNX_PLATFORM_CONSTANTS`, which
the host installs from the running system via `QSysInfo`. The module throws if
that global is absent rather than reporting invented values.

`BaseViewConfig` carries the platform-independent bubbling and direct event
tables plus React Native's own `ReactNativeStyleAttributes` for
`validAttributes`. The iOS file's accessibility and force-touch events are
deliberately absent rather than declared and unimplemented.

## Text

React Native selects a `TextLayoutManager` per platform, and its generic C++ one
does not measure: it returns `layoutConstraints.minimumSize`
(`textlayoutmanager/platform/cxx/.../TextLayoutManager.cpp:20-33`), which
collapses every `<Text>` to nothing. Android and Apple each substitute a real
implementation, so this platform does the same — React Native's header
unchanged, the definitions replaced, measured with `QFontMetricsF`.

| Seam                                    | Installed authority                                             |
| --------------------------------------- | --------------------------------------------------------------- |
| `TextLayoutManager` declaration         | `textlayoutmanager/platform/cxx/.../TextLayoutManager.h`        |
| `TextLayoutManagerKey` in the container | `components/text/BaseParagraphComponentDescriptor.h:16,23`      |
| `ParagraphState::attributedString`      | `components/text/platform/cxx/.../ParagraphState.h:21-26`       |
| Paragraph/Text/RawText descriptors      | `components/text/{Paragraph,Text,RawText}ComponentDescriptor.h` |

Three descriptors are registered, not one: `Paragraph` is the laid-out block,
`Text` a nested span, and `RawText` the string itself. The descriptor pulls the
layout manager out of the `ContextContainer` by name, so a missing entry throws
at construction rather than degrading.

Qt Quick's `Text` styles the whole item, so only the first fragment's font size
and colour are carried; mixed styling inside one paragraph collapses. Recorded
in `docs/component-support.md`.

## Input

React Native ships no generic input path: Android reads Choreographer-driven
motion events and Apple reads UIKit touches. Qt delivers pointer events to the
window, so `QtTouchDispatcher` filters them there, hit-tests to the deepest
mounted item, and calls that shadow view's emitter.

| Seam                                        | Installed authority                                |
| ------------------------------------------- | -------------------------------------------------- |
| `TouchEventEmitter::onTouchStart/Move/End`  | `components/view/TouchEventEmitter.h:28-31`        |
| `TouchEvent` (touches/changed/target lists) | `components/view/TouchEvent.h:20-38`               |
| `Touch` fields                              | `components/view/BaseTouch.h:26,31,36,41,48,53,59` |
| `ShadowView::eventEmitter`                  | `mounting/ShadowView.h:45`                         |

Scope is one pointer — press, move, release. Multi-touch, pointer capture, and
the JS responder negotiation are not implemented, and
`docs/component-support.md` says so rather than implying `Pressable` fully works.

A lifetime rule worth stating: the adapter borrows the dispatcher, and stopping
a surface commits an empty tree _during host teardown_, which reaches the
adapter and therefore the dispatcher. The dispatcher must outlive the host, or
be cleared first. Getting this wrong corrupts the heap rather than failing
cleanly.

## Host responsibilities beyond the bindings

Hermes is a JavaScript engine, not a runtime, so several things JavaScript
assumes exist are the host's job:

| Responsibility  | Why                                                                                                                                                                |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Microtask queue | off by default; `RuntimeConfig::Builder().withMicrotaskQueue(true)`. `NativeMicrotasks` forwards straight to `Runtime::queueMicrotask`, which throws when disabled |
| Timers          | `setTimeout` and friends are not engine builtins. Backed by `QTimer`, so they fire only while the Qt event loop runs                                               |
| Draining        | React schedules through microtasks and the RuntimeScheduler; with no running loop the host drains explicitly                                                       |

## TurboModule adapter (ADR-0004's sanctioned exception)

Application-facing APIs are Nitro HybridObjects. React Native's own core
JavaScript, however, calls `TurboModuleRegistry.getEnforcing` for its own
modules, and nothing else can answer that registry. ADR-0004 anticipated this:
"A minimal internal adapter satisfies React Native core interfaces that expect
TurboModule registry semantics, but the adapter delegates real behavior to Nitro
or the platform runtime," with the note "Do not generate empty TurboModules
merely to silence linker errors."

The adapter is one call:

| Seam                                         | Installed authority                                              |
| -------------------------------------------- | ---------------------------------------------------------------- |
| `TurboModuleBinding::install`                | `react/nativemodule/core/ReactCommon/TurboModuleBinding.h:41-45` |
| `TurboModuleProviderFunctionTypeWithRuntime` | `react/nativemodule/core/ReactCommon/TurboModule.h:152-153`      |
| `DefaultTurboModules::getTurboModule`        | `react/nativemodule/defaults/DefaultTurboModules.h:12-16`        |
| `RuntimeSchedulerCallInvoker`                | `react/renderer/runtimescheduler/RuntimeSchedulerCallInvoker.h`  |

`install` branches on `RN$Bridgeless` (`TurboModuleBinding.cpp:124`): with the
flag set it installs `nativeModuleProxy`; without it, `__turboModuleProxy`,
which `Libraries/TurboModule/TurboModuleRegistry.js:17` captures **once at
module-eval time** — so the binding must be installed before any bundle runs.

The provider delegates to `DefaultTurboModules::getTurboModule`, which returns
React Native's own generic C++ modules — real implementations backed by the
platform runtime. Two of the nine the playground bundle demands come from there
already written: `NativeMicrotasksCxx` and `NativeIdleCallbacksCxx`, the latter
requiring a bound `RuntimeScheduler`. Anything not covered returns nullptr, so
`getEnforcing` fails naming the module rather than receiving an empty stub.

Of the remaining seven: DeviceInfo, ExceptionsManager, FileReaderModule,
SourceCode, and WebSocketModule have generated `…CxxSpec` base classes in the
shipped `FBReactNativeSpecJSI.h` but Apple/Android-only implementations;
SegmentFetcher has a JS spec and no native implementation anywhere in the
package; and **UIManager is not a TurboModule at all** — it has no codegen spec,
and bridgeless React Native serves it from JavaScript via
`Libraries/ReactNative/BridgelessUIManager.js`.

## Qt and React Native header collision

Qt defines `emit`, `signals`, and `slots` as macros. React Native's
`react/bridging/EventEmitter.h` declares a method named `emit`, which the macro
shreds into a syntax error the moment both headers meet. `QT_NO_KEYWORDS` turns
the macros off; nothing here uses them, and `Q_EMIT`/`Q_SIGNALS` remain
available if a `Q_OBJECT` class ever needs them.

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

- The Qt 6.11.1 source is not installed. `QQuickItem`, `QQuickWindow`,
  `QQmlComponent`, QPA implementation paths, plugin filenames, and private/public
  boundaries are **not yet authorized implementation seams**.
- Qt module licensing is cited from
  [Qt 6.11 Licensing](https://doc.qt.io/qt-6.11/licensing.html) and recorded in
  ADR-0011; it must be re-checked against the pinned source tree.
- The `qtbase` keg contains `FindSlog2.cmake` and `FindPPS.cmake` under
  `lib/cmake/Qt6` and `mkspecs` mentions `qqnx_imf`, `slog2`, and `qqnx_pps` as
  disabled features on macOS. These confirm QNX-specific symbols are present in
  the Qt source tree; they are not macOS build evidence.
- The `qt` keg contains `QtQuickControls2Basic`, `QtQuickControls2Fusion`,
  `QtQuickControls2Material`, `QtQuickControls2Universal`, and
  `QtQuickControls2Imagine` frameworks, plus `QtLabsStyleKit` (Technology
  Preview). ADR-0012 selects `Basic` as the default style.
- Installed Expo Modules Core has Apple/Android runtime installers only; no QNX
  `globalThis.expo` installer or registry exists.
- Installed Expo Metro defaults omit `qnx`; adding a resolver platform is a
  future source-backed design, not current functionality.

## Shared bundled React geometry probe (2026-10-04)

- React Native 0.88.0-rc.3 installed source:
  `Libraries/ReactNative/AppRegistryImpl.js::registerComponent` and
  `ReactCommon/react/renderer/uimanager/AppRegistryBinding.cpp::startSurface/stopSurface`
  define the real bundle registration and surface lifecycle. The fixture uses
  installed React 19.3.0 `useState` and the RN View component, not a hand-written
  Fabric tree. The production Metro bundle was built successfully.
- Existing platform interface:
  `vnext/include/react-native-qnx/runtime/QtReactHost.h::evaluate/startSurface/stopSurface/pumpEvents`
  and `QtSchedulerDelegate::mountedTransactionCount/lastFailures`; the probe
  consumes these without injecting native nodes or success globals.
- Qt 6.11.1 installed host headers:
  `QtQuick/qquickitem.h::childItems/x/y/width/height`,
  `QtCore/qobject.h::property`, `QtCore/qelapsedtimer.h::start/elapsed`, and
  `QtCore/qthread.h::msleep` back the geometry/property assertions and bounded
  event pumping. `QtGui/qcolor.h` supplies exact color comparison.
- Qt 6.11.1 `QtQuick/private/qquickrectangle_p.h` declares corner radii as
  inherited FINAL properties. The fixture must not redeclare those properties;
  doing so prevents the QML View from being created.
- Evidence level: source/header verified, Metro bundled, and standalone native
  positive/four-negative runners pass with companion host PR #28 `24ba958`.
  The actual shared target passed 2/2 CTests; the combined host graph passed 11/11. QNX header/build/device and
  pixel evidence are separate pending gates. No new Qt module is linked.

## Image loader HTTP, decoded cache and Promise seam (2026-10-04)

Inspected installed Qt 6.11.1 Linux headers under `include/`, and the installed
React Native 0.88.0-rc.3 package, before implementing these calls:

- `QtNetwork/qnetworkaccessmanager.h`: `QNetworkAccessManager::get`;
  `QtNetwork/qnetworkrequest.h`: `setRawHeader`, `setTransferTimeout`,
  `RedirectPolicyAttribute`/`ManualRedirectPolicy`, `HttpStatusCodeAttribute`,
  `CookieLoadControlAttribute`, `CookieSaveControlAttribute`,
  `AuthenticationReuseAttribute` and `Manual`.
- `QtNetwork/qnetworkreply.h`: `readyRead` (QIODevice), `finished`, `error`,
  `rawHeader`, `abort`, `setReadBufferSize`; `QtCore/qtimer.h`:
  `setSingleShot`, `start`, `timeout`; `QtCore/qobject.h`: context-bound
  `connect` and `deleteLater`.
- `QtGui/qimagereader.h`: `size`, `read`; `QtGui/qimage.h`: `sizeInBytes`;
  `QtCore/qbuffer.h`, `qfile.h`, `qbytearray.h`: bounded input and
  `AbortOnBase64DecodingErrors`; `QtCore/qcache.h`: bounded cost/LRU storage;
  `QtCore/qmutex.h`: `QMutexLocker`; `QtCore/qcryptographichash.h`: SHA-256 keys.
- `QtQuick/qquickimageprovider.h`: `requestImage`; existing provider/factory
  reuse path. `QtNetwork/qtcpserver.h`, `qtcpsocket.h` are used only by the
  deterministic local HTTP test.
- React Native `ReactCommon/react/bridging/Promise.h`: `AsyncPromise`, `get`,
  `resolve`, `reject`; `Dynamic.h`: `Bridging<folly::dynamic>`; `Function.h`:
  `AsyncCallback`; `Error.h`: actual JS Error rejection. The callback wrapper
  retains JS functions through `LongLivedObjectCollection` and schedules them
  through `CallInvoker`, rather than capturing a raw JSI runtime.
- React Native `ReactCommon/react/nativemodule/core/ReactCommon/TurboModuleBinding.cpp`:
  `install` selects `nativeModuleProxy` in bridgeless mode; the Hermes contract
  test uses that real module lookup.
- React Native `src/private/specs_DEPRECATED/modules/NativeImageLoaderIOS.js`:
  method names, `[width,height]` versus `{width,height}` results, metadata
  arguments and Promise query-cache map. This QNX core adapter preserves that
  existing contract; it does not introduce application TurboModules.

Network module target measurements are pending in `docs/build-qt-modules.md`.

## Worklets queue and frame shutdown (2026-11-14)

Inspected installed Qt 6.11.1 Linux headers under the official desktop prefix:

| Header                           | Public symbols used                                                                          | SHA-256                                                            |
| -------------------------------- | -------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| `include/QtCore/qobjectdefs.h`   | `QMetaObject::invokeMethod(QObject*, Functor, Qt::ConnectionType)`; explicit queued delivery | `68c7065f0a09d621977a14bd11b6afe5bc9deac6c09f55707d7cfdc27e946724` |
| `include/QtCore/qthread.h`       | `QThread::quit`, `QThread::wait`                                                             | `857d3c4190c9e43da0286a71399589246ee583c5eba77f8882448680c6802a2a` |
| `include/QtQuick/qquickwindow.h` | `QQuickWindow::afterAnimating`, `QQuickWindow::update`                                       | `5ec33fa8024def05b546a611e648f594b870d69ece9252c08d9e31c8b72fd88a` |

The existing `QtWorkletsPlatform::shutdown` closes the frame bridge before
joining the UI thread and destroying the bridge. Admission mutexes pin each
receiver through enqueueing; they do not guard callback execution, QObject
destruction or join. A closed receiver discards queued calls, and explicit
shutdown checks prevent unstarted frame callbacks from entering the runtime.
The standalone real-Qt tests exercise those behaviors under scoped ASan/UBSan.
This is installed-header/host evidence only; QNX target validation of this
follow-up is not run. See [the receipt](validation/pr149-shutdown-2026-11-14.md).

## Image worker dispatch and completion (2026-11-14)

Inspected the installed official Linux Qt 6.11.1 headers under
`$QT_HOST_PREFIX/include/QtCore/` (`qconfig.h:67` identifies `6.11.1`). These
entries cover the worker pipeline already used by `QtImageFetcher`, including
the disk-cache generation guard added during PR 159 review.

| Symbols                                                          | Installed authority                   | Evidence and boundary                                                                                                                                                                                |
| ---------------------------------------------------------------- | ------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `QThreadPool::globalInstance`, callable `start`                  | `qthreadpool.h:35,50-51,89-93`        | Installed-header verified; the callable becomes a `QRunnable`. The fetch queue bounds admission; worker captures contain values and shared cache state, no QObject or JSI values.                    |
| `QPromise<T>::future`, `addResult`, `start`, `finish`            | `qpromise.h:49,60-64,75-76`           | Installed-header verified; each worker publishes one result and finishes its promise.                                                                                                                |
| `QFutureWatcher<T>(QObject*)`, `finished`, `result`, `setFuture` | `qfuturewatcher.h:52,103-114,176-184` | Installed-header verified; the watcher is parented to the fetcher and its completion is connected with the fetcher as context. Owner-thread completion still checks that the pending request exists. |
| Context-bound `QObject::connect`, `deleteLater`                  | `qobject.h:228,233-279,359`           | Installed-header verified; owner destruction disconnects completion, and a completed watcher is disposed through the event loop.                                                                     |

Host compile/runtime results are recorded separately in
[the review receipt](validation/pr159-review-followup-2026-11-14.md). These header
citations establish no QNX SDP, target Qt ABI, device, or performance result.

The animated memory-hit path reuses the view's existing movie ownership code.
Installed Qt 6.11.1 `QtGui/qmovie.h:47,71,88,95,98,101,103` declares the
`QMovie(QIODevice*)` constructor, `isValid`, `setCacheMode`, `stateChanged`,
`frameChanged`, `start` and `setPaused`. The view owns its QBuffer and QMovie on
the GUI thread; cache entries contain only shared QImage/QByteArray values.

## BlurHash maximum coefficient (2026-11-14)

The independently compiled `woltapp/blurhash` C reference computes the AC scale
from `fmaxf(fabsf(ac[i]), actualMaximumValue)` in `C/encode.c`
(Git blob `c7a39da2a583231e45b7c22526ca688655a0c923`). `C/encode.h` and
`C/common.h` blobs are recorded beside the resulting vectors in
`tests/fixtures/image-hashes/reference-vectors.json`. The C encoder outputs
cover all 16 original images/component combinations and a four-pixel gradient
with a dominant negative coefficient. The historical `blurhash@2.0.5` npm
outputs remain in the fixture: that TypeScript encoder uses signed maxima and
differs on three noise-image cases. Current encoding conformance uses the C
reference; decoder and ThumbHash reference coverage is unchanged.

Source: [official C encoder](https://github.com/woltapp/blurhash/blob/master/C/encode.c).
