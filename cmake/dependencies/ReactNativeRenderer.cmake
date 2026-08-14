# Builds the React Native 0.86.2 renderer modules needed to run a real shadow
# tree, using React Native's own per-module CMakeLists rather than a rewritten
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
)
  include_directories("${REACT_COMMON_DIR}/${platform_dir}")
endforeach()

set(RNQNX_REACT_NATIVE_MODULES
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
  react/renderer/leakchecker
  react/renderer/textlayoutmanager
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

foreach(module IN LISTS RNQNX_REACT_NATIVE_MODULES)
  add_subdirectory(
    "${REACT_COMMON_DIR}/${module}"
    "${CMAKE_BINARY_DIR}/react-native/${module}"
  )
endforeach()

# ReactCommon/yoga/yoga/CMakeLists.txt names its target yogacore; every renderer
# module links it as `yoga`.
add_library(yoga ALIAS yogacore)

# One target that carries every renderer module. Most of them are CMake OBJECT
# libraries, whose objects only reach an executable when the executable links
# them, so consumers link this aggregate instead of guessing a subset.
add_library(rnqnx_react_native_renderer INTERFACE)
set(RNQNX_REACT_NATIVE_RENDERER_TARGETS
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
  react_renderer_leakchecker
  react_renderer_textlayoutmanager
  react_renderer_attributedstring
  rrc_text
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
