# Builds the renderer modules from the installed React Native package needed
# to run a real shadow tree, using React Native's own per-module CMakeLists rather
# than a rewritten
# build. Each module directory under ReactCommon ships an add_library plus its
# link list; adding those directories keeps this file honest, because the source
# lists and flags stay owned by React Native.
#
# The modules link against third-party targets by the names React Native's
# Android build uses, so those names are provided here as aliases over the
# targets defined in ReactNativeThirdParty.cmake.

include_guard(GLOBAL)

if(NOT TARGET rnqnx_folly_runtime)
  message(FATAL_ERROR
    "ReactNativeThirdParty.cmake must be included before ReactNativeRenderer.cmake")
endif()

set(REACT_COMMON_DIR "${RNQNX_REACT_NATIVE_DIR}/ReactCommon")

# This repository root, for sources outside node_modules.
get_filename_component(RNQNX_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
# Some modules include React Native's Android-side CMake helpers. The helper
# they need, target_merge_so, is a no-op off Android (SoMerging-utils.cmake:17),
# so pointing at the real directory is enough; nothing Android-specific runs.
set(REACT_ANDROID_DIR "${RNQNX_REACT_NATIVE_DIR}/ReactAndroid")

# React Native's own platform selector. With ANDROID unset it picks each
# module's generic `cxx` implementation, which is the same path QNX will take.
include("${REACT_COMMON_DIR}/cmake-utils/internal/react-native-platform-selector.cmake")

# React Native's expected third-party target names.
add_library(folly_runtime ALIAS rnqnx_folly_runtime)
add_library(glog ALIAS rnqnx_glog)
add_library(fmt ALIAS rnqnx_fmt)
add_library(fast_float ALIAS rnqnx_fast_float)
add_library(double-conversion ALIAS rnqnx_double_conversion)
add_library(boost ALIAS rnqnx_boost)

# glog_init is React Native's Android-only glog bootstrap
# (ReactAndroid/src/main/jni/first-party/glog-init). Nothing on this host needs
# Android's logging redirection, so the name resolves to an empty interface
# rather than to a stub implementation that could be mistaken for real work.
add_library(rnqnx_glog_init INTERFACE)
add_library(glog_init ALIAS rnqnx_glog_init)

# jsinspector tracing is pulled in by the mounting module for performance
# traces. The real module is built below; this guard exists so a future
# reduction of the module list fails loudly rather than silently linking
# nothing.
# React Native's Android assembler puts every platform-selected include
# directory on the global include path, and several modules depend on that: for
# example react_renderer_componentregistry compiles ComponentDescriptorRegistry
# .cpp, which reaches ViewProps.h -> HostPlatformViewProps.h without linking
# rrc_view. These are the generic `cxx` variants, which is what a non-Android
# platform such as QNX uses.
# The generated module specs are addressed as
# <FBReactNativeSpec/FBReactNativeSpecJSI.h>, and React Native ships that
# pre-generated header under React/ in the Apple pod layout even though it is
# plain C++/JSI. Same artifact Android calls react_codegen_rncore.
include_directories(SYSTEM "${RNQNX_REACT_NATIVE_DIR}/React")
include_directories(SYSTEM "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi")
include_directories(SYSTEM "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/callinvoker")

# Third-party include roots the React Native module targets assume are global,
# the way React Native's own Android assembler provides them.
include_directories(SYSTEM
  "${rnqnx_folly_SOURCE_DIR}"
  "${RNQNX_GLOG_EXPORTED}"
  "${rnqnx_fmt_SOURCE_DIR}/include"
  "${rnqnx_fast_float_SOURCE_DIR}/include"
  "${RNQNX_DOUBLE_CONVERSION_STAGE}"
  "${rnqnx_boost_SOURCE_DIR}"
  "${rnqnx_boost_config_SOURCE_DIR}/include"
)

foreach(platform_dir IN ITEMS
  react/renderer/components/modal/platform/cxx
  react/renderer/components/scrollview/platform/cxx
  react/renderer/components/text/platform/cxx
  react/renderer/components/view/platform/cxx
  react/renderer/graphics/platform/cxx
  react/renderer/imagemanager/platform/cxx
  react/renderer/textlayoutmanager/platform/cxx
  react/utils/platform/cxx
  runtimeexecutor/platform/cxx
  # TurboModule headers are addressed as <ReactCommon/TurboModule.h>, which
  # resolves from the module's own root rather than from ReactCommon.
  react/nativemodule/core
)
  include_directories(SYSTEM "${REACT_COMMON_DIR}/${platform_dir}")
endforeach()

set(RNQNX_REACT_NATIVE_MODULES
  react/cxxstableapi
  jserrorhandler
  react/renderer/observers/resize
  react/nativemodule/resizeobserver
  react/debug
  callinvoker
  react/utils
  react/featureflags
  react/renderer/debug
  react/renderer/graphics
  react/renderer/mapbuffer
  react/renderer/telemetry
  react/renderer/runtimescheduler
  react/renderer/core
  react/renderer/componentregistry
  react/renderer/components/view
  react/renderer/components/root
  react/renderer/components/scrollview
  react/renderer/mounting
  react/renderer/consistency
  react/renderer/observers/events
  react/performance/timeline
  react/timing
  react/renderer/components/image
  react/renderer/imagemanager
  react/renderer/animated
  react/renderer/observers/mutation
  react/renderer/observers/intersection
  devtoolsruntimesettings
  react/bridging
  react/nativemodule/core
  react/nativemodule/microtasks
  react/nativemodule/idlecallbacks
  react/nativemodule/dom
  react/nativemodule/featureflags
  react/nativemodule/devtoolsruntimesettings
  react/nativemodule/intersectionobserver
  react/nativemodule/mutationobserver
  react/nativemodule/viewtransition
  react/nativemodule/webperformance
  react/nativemodule/defaults
  react/renderer/componentregistry/native
  react/renderer/viewtransition
  react/renderer/animationbackend
  react/performance/cdpmetrics
  react/renderer/components/unimplementedview
  react/renderer/scheduler
  react/renderer/element
  react/renderer/leakchecker
  react/renderer/attributedstring
  react/renderer/components/text
  react/renderer/dom
  react/renderer/bridging
  react/renderer/uimanager/consistency
  cxxreact
  react/renderer/uimanager
  react/renderer/components/legacyviewmanagerinterop
  react/renderer/css
  jsinspector-modern
  oscompat
  jsinspector-modern/cdp
  jsinspector-modern/network
  reactperflogger
  jsinspector-modern/tracing
  jsi
  runtimeexecutor
  logger
  yoga
)

# Hermes is configured against React Native's JSI copy (JSI_DIR), so when Hermes
# is built it has already added that directory and created the `jsi` target.
# Adding it twice is a duplicate-target error, and pointing Hermes at its own
# bundled JSI instead would put two jsi implementations in one process.
set(RNQNX_JSI_PROVIDED_BY_HERMES FALSE)
foreach(module IN LISTS RNQNX_REACT_NATIVE_MODULES)
  if(module STREQUAL "jsi" AND TARGET jsi)
    set(RNQNX_JSI_PROVIDED_BY_HERMES TRUE)
    continue()
  endif()
  add_subdirectory(
    "${REACT_COMMON_DIR}/${module}"
    "${CMAKE_BINARY_DIR}/react-native/${module}"
  )

endforeach()

# RN 0.88's redbox AnsiParser.h uses uint8_t without including <cstdint>.
# libc++ supplies it transitively; libstdc++ does not. Supply the standard
# header for this one upstream translation unit without editing node_modules.
set_property(SOURCE "${REACT_COMMON_DIR}/react/debug/redbox/AnsiParser.cpp"
  TARGET_DIRECTORY react_debug APPEND PROPERTY COMPILE_OPTIONS
  "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-include;cstdint>")

# `react_codegen_rncore` is codegen output, so it has no CMakeLists to add.
# ReactCommon/react/renderer/components/rncore/ holds five headers that each
# forward to FBReactNativeSpec, and FBReactNativeSpec does not exist under
# ReactCommon at all — but React Native does ship a pre-generated copy under
# React/FBReactNativeSpec, and its renderer half is plain C++ despite living in
# the Apple tree. Building those five sources satisfies every module that links
# rncore (scheduler, animationbackend, viewtransition).
#
# This is a stopgap with a known ceiling: the artifacts are Apple-flavoured
# generation, and anything beyond React Native's core components needs the
# codegen run properly. Recorded in docs/cited-seams.md.
set(RNQNX_RNCORE_DIR "${RNQNX_REACT_NATIVE_DIR}/React/FBReactNativeSpec")
add_library(react_codegen_rncore STATIC
  "${RNQNX_RNCORE_DIR}/react/renderer/components/FBReactNativeSpec/ComponentDescriptors.cpp"
  "${RNQNX_RNCORE_DIR}/react/renderer/components/FBReactNativeSpec/EventEmitters.cpp"
  "${RNQNX_RNCORE_DIR}/react/renderer/components/FBReactNativeSpec/Props.cpp"
  "${RNQNX_RNCORE_DIR}/react/renderer/components/FBReactNativeSpec/ShadowNodes.cpp"
  "${RNQNX_RNCORE_DIR}/react/renderer/components/FBReactNativeSpec/States.cpp"
)
target_include_directories(react_codegen_rncore SYSTEM PUBLIC "${RNQNX_RNCORE_DIR}")
target_link_libraries(react_codegen_rncore PUBLIC
  react_renderer_core
  react_renderer_graphics
  rrc_view
  yogacore
  rnqnx_folly
)
target_compile_options(react_codegen_rncore PRIVATE -w)

# React Native selects a TextLayoutManager per platform, and its generic C++ one
# does not measure: it returns layoutConstraints.minimumSize, collapsing every
# <Text> to nothing. Android and Apple each substitute a real implementation, so
# this target is assembled here with ours in place of the generic .cpp. React
# Native's header and its other sources are used unchanged.
set(RNQNX_TEXTLAYOUT_DIR "${REACT_COMMON_DIR}/react/renderer/textlayoutmanager")
file(GLOB RNQNX_TEXTLAYOUT_SRC CONFIGURE_DEPENDS "${RNQNX_TEXTLAYOUT_DIR}/*.cpp")
add_library(react_renderer_textlayoutmanager OBJECT
  ${RNQNX_TEXTLAYOUT_SRC}
  "${RNQNX_REPO_ROOT}/vnext/src/text/QtTextLayoutManager.cpp"
)
target_include_directories(react_renderer_textlayoutmanager PUBLIC
  "${RNQNX_TEXTLAYOUT_DIR}"
  "${REACT_COMMON_DIR}"
  "${RNQNX_TEXTLAYOUT_DIR}/platform/cxx"
)
target_link_libraries(react_renderer_textlayoutmanager
  react_renderer_attributedstring
  react_renderer_core
  react_renderer_graphics
  react_utils
  rnqnx_folly
  Qt6::Gui
)
target_compile_options(react_renderer_textlayoutmanager PRIVATE -w)

# React Native's own textinput CMakeLists globs the Android platform sources,
# which need fbjni and the Java FabricUIManager
# (AndroidTextInputComponentDescriptor.h:41-47). The sources under
# `platform/ios/.../iostextinput` have no Apple dependency — TextInputShadowNode
# and TextInputProps include only react/, yoga and the standard library — so
# this target pairs them with the shared base, the same substitution Android and
# Apple each perform. `TextInputComponentName` is "TextInput"
# (TextInputShadowNode.cpp:12), which is what React Native's own JavaScript view
# names map onto (componentNameByReactViewName.cpp:62-64).
set(RNQNX_TEXTINPUT_DIR "${REACT_COMMON_DIR}/react/renderer/components/textinput")
set(RNQNX_TEXTINPUT_IOS_DIR
  "${RNQNX_TEXTINPUT_DIR}/platform/ios/react/renderer/components/iostextinput")
file(GLOB RNQNX_TEXTINPUT_SRC CONFIGURE_DEPENDS
  "${RNQNX_TEXTINPUT_DIR}/*.cpp"
  "${RNQNX_TEXTINPUT_IOS_DIR}/*.cpp"
)
add_library(rrc_textinput OBJECT ${RNQNX_TEXTINPUT_SRC})
# SYSTEM: iostextinput/conversions.h has unused `context` parameters on its
# fromRawValue overloads, which this repository builds as errors. The header is
# included by our own strictly-warned targets through the component descriptor,
# so suppressing it at the source target is the only place it can be done.
target_include_directories(rrc_textinput SYSTEM PUBLIC
  "${RNQNX_TEXTINPUT_DIR}"
  "${RNQNX_TEXTINPUT_DIR}/platform/ios"
  "${REACT_COMMON_DIR}"
)
target_link_libraries(rrc_textinput
  react_renderer_attributedstring
  react_renderer_componentregistry
  react_renderer_core
  react_renderer_graphics
  react_renderer_imagemanager
  react_renderer_mounting
  react_renderer_textlayoutmanager
  react_renderer_uimanager
  react_utils
  rnqnx_folly
  rrc_image
  rrc_text
  rrc_view
)
target_compile_options(rrc_textinput PRIVATE -w)

# iOS switch descriptor: the .mm sources are Apple-only, so this target uses
# a QNX SwitchShadowNode.cpp that defines the component name and a default
# intrinsic size. `SwitchComponentName` is "Switch".
set(RNQNX_SWITCH_DIR "${REACT_COMMON_DIR}/react/renderer/components/switch")
set(RNQNX_SWITCH_IOS_DIR
  "${RNQNX_SWITCH_DIR}/iosswitch/react/renderer/components/switch")
add_library(rrc_switch OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/vnext/src/fabric/SwitchShadowNode.cpp")
target_include_directories(rrc_switch SYSTEM PUBLIC
  "${RNQNX_SWITCH_DIR}"
  "${RNQNX_SWITCH_IOS_DIR}"
  "${REACT_COMMON_DIR}"
)
target_link_libraries(rrc_switch
  react_codegen_rncore
  react_renderer_componentregistry
  react_renderer_core
  react_renderer_graphics
  rrc_view
  react_utils
  rnqnx_folly
  yogacore
)
target_compile_options(rrc_switch PRIVATE -w)

# SafeAreaView is a portable shadow node: it is just a View whose insets are
# expected to come from the platform (zero on QNX). Its own CMake target in
# React Native pulls in Android-only dependencies, so this repo builds the
# two .cpp files it actually needs directly.
set(RNQNX_SAFEAREAVIEW_DIR
  "${REACT_COMMON_DIR}/react/renderer/components/safeareaview")
add_library(rrc_safeareaview OBJECT
  "${RNQNX_SAFEAREAVIEW_DIR}/SafeAreaViewShadowNode.cpp"
  "${RNQNX_SAFEAREAVIEW_DIR}/SafeAreaViewState.cpp")
target_include_directories(rrc_safeareaview SYSTEM PUBLIC
  "${RNQNX_SAFEAREAVIEW_DIR}"
  "${REACT_COMMON_DIR}"
)
target_link_libraries(rrc_safeareaview
  react_codegen_rncore
  react_renderer_componentregistry
  react_renderer_core
  react_renderer_graphics
  rrc_view
  react_utils
  rnqnx_folly
  yogacore
)
target_compile_options(rrc_safeareaview PRIVATE -w)

# Modal is a portable shadow node; its own upstream target is not built by
# the dependencies included in this project, so it is assembled here.
set(RNQNX_MODAL_DIR "${REACT_COMMON_DIR}/react/renderer/components/modal")
add_library(rrc_modal OBJECT
  "${RNQNX_MODAL_DIR}/ModalHostViewComponentDescriptor.cpp"
  "${RNQNX_MODAL_DIR}/ModalHostViewShadowNode.cpp"
  "${RNQNX_MODAL_DIR}/ModalHostViewState.cpp"
  "${RNQNX_MODAL_DIR}/platform/cxx/ModalHostViewUtils.cpp")
target_include_directories(rrc_modal SYSTEM PUBLIC
  "${RNQNX_MODAL_DIR}"
  "${REACT_COMMON_DIR}"
)
target_link_libraries(rrc_modal
  react_codegen_rncore
  react_renderer_componentregistry
  react_renderer_core
  react_renderer_graphics
  react_renderer_imagemanager
  rrc_image
  rrc_view
  rnqnx_folly
  yogacore
)
target_compile_options(rrc_modal PRIVATE -w)

# React Native ships two CMakeLists for JSI and they are not equivalent:
# ReactCommon/jsi/CMakeLists.txt:17 globs `jsi/*.cpp`, so it includes
# JSIDynamic.cpp, while ReactCommon/jsi/jsi/CMakeLists.txt:9 builds jsi.cpp
# alone. Hermes adds the inner one, so when Hermes provides `jsi` the
# folly::dynamic bridge is missing and AppRegistryBinding fails to link. Adding
# that one translation unit is the whole difference.
if(RNQNX_JSI_PROVIDED_BY_HERMES)
  add_library(rnqnx_jsi_dynamic STATIC
    "${REACT_COMMON_DIR}/jsi/jsi/JSIDynamic.cpp")
  target_link_libraries(rnqnx_jsi_dynamic PUBLIC jsi rnqnx_folly)
  target_include_directories(rnqnx_jsi_dynamic PUBLIC "${REACT_COMMON_DIR}/jsi")
endif()

# ReactCommon/yoga/yoga/CMakeLists.txt names its target yogacore; every renderer
# module links it as `yoga`.
add_library(yoga ALIAS yogacore)

# One target that carries every renderer module. Most of them are CMake OBJECT
# libraries, whose objects only reach an executable when the executable links
# them, so consumers link this aggregate instead of guessing a subset.
add_library(rnqnx_react_native_renderer INTERFACE)
set(RNQNX_REACT_NATIVE_RENDERER_TARGETS
  react_cxxstableapi
  jserrorhandler
  react_renderer_observers_resize
  react_nativemodule_resizeobserver
  react_debug
  react_utils
  react_featureflags
  react_renderer_debug
  react_renderer_graphics
  react_renderer_mapbuffer
  react_renderer_telemetry
  react_renderer_runtimescheduler
  react_renderer_core
  react_renderer_componentregistry
  react_renderer_consistency
  react_renderer_observers_events
  react_renderer_css
  rrc_view
  rrc_root
  rrc_scrollview
  react_renderer_mounting
  react_performance_timeline
  react_timing
  rrc_image
  react_renderer_imagemanager
  react_renderer_animated
  react_renderer_observers_mutation
  react_renderer_observers_intersection
  devtoolsruntimesettings
  react_bridging
  react_nativemodule_core
  react_nativemodule_microtasks
  react_nativemodule_idlecallbacks
  react_nativemodule_dom
  react_nativemodule_featureflags
  react_nativemodule_devtoolsruntimesettings
  react_nativemodule_intersectionobserver
  react_nativemodule_mutationobserver
  react_nativemodule_viewtransition
  react_nativemodule_webperformance
  react_nativemodule_defaults
  rrc_native
  react_renderer_viewtransition
  react_codegen_rncore
  react_renderer_animationbackend
  react_performance_cdpmetrics
  rrc_unimplementedview
  react_renderer_scheduler
  react_renderer_element
  react_renderer_leakchecker
  react_renderer_textlayoutmanager
  react_renderer_attributedstring
  rrc_text
  rrc_textinput
  rrc_switch
  rrc_safeareaview
  rrc_modal
  react_renderer_dom
  react_renderer_bridging
  react_renderer_uimanager_consistency
  react_cxxreact
  react_renderer_uimanager
  rrc_legacyviewmanagerinterop
  jsinspector
  jsinspector_cdp
  jsinspector_network
  jsinspector_tracing
  reactperflogger
  oscompat
  callinvoker
  runtimeexecutor
  logger
  jsi
  yogacore
  rnqnx_folly_runtime
  rnqnx_react_native_headers
)
if(RNQNX_JSI_PROVIDED_BY_HERMES)
  list(APPEND RNQNX_REACT_NATIVE_RENDERER_TARGETS rnqnx_jsi_dynamic)
endif()
target_link_libraries(rnqnx_react_native_renderer INTERFACE
  ${RNQNX_REACT_NATIVE_RENDERER_TARGETS}
)

# Most React Native modules are CMake OBJECT libraries. Their object files are
# not archived into a static library that links them, so a consumer three steps
# down the chain ends up with usage requirements and no code. Attaching the
# objects to this interface target puts them in every consumer's link line.
foreach(rn_target IN LISTS RNQNX_REACT_NATIVE_RENDERER_TARGETS)
  get_target_property(rn_target_type ${rn_target} TYPE)
  if(rn_target_type STREQUAL "OBJECT_LIBRARY")
    target_sources(rnqnx_react_native_renderer INTERFACE
      $<TARGET_OBJECTS:${rn_target}>)
  endif()
endforeach()
