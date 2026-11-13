include_guard(GLOBAL)

# react-native-reanimated v4 on Qt, on top of the Worklets target. Default OFF.
#
# Like Worklets, the upstream C++ is compiled from an installed package and is
# not vendored. Two upstream lines are text-patched in generated copies (a
# platform type that only Android and Apple define, and the loop that applies
# synchronous props on Apple); the configure step fails if either line changes.
# The Fabric codegen output the library's headers need is generated into the
# build directory from the package's own specs.

option(RNQNX_ENABLE_REANIMATED "Build react-native-reanimated v4 on the Qt Worklets platform" OFF)
set(RNQNX_REANIMATED_SOURCE_DIR "" CACHE PATH "Installed react-native-reanimated package root")

function(rnqnx_add_reanimated_target)
  if(TARGET rnqnx_reanimated)
    return()
  endif()
  if(NOT TARGET rnqnx_worklets)
    message(FATAL_ERROR "RNQNX_ENABLE_REANIMATED needs RNQNX_ENABLE_WORKLETS")
  endif()
  if(NOT RNQNX_REANIMATED_SOURCE_DIR)
    set(RNQNX_REANIMATED_SOURCE_DIR "${CMAKE_SOURCE_DIR}/node_modules/react-native-reanimated")
  endif()
  get_filename_component(pkg "${RNQNX_REANIMATED_SOURCE_DIR}" REALPATH)
  if(NOT EXISTS "${pkg}/package.json" OR NOT IS_DIRECTORY "${pkg}/Common/cpp/reanimated")
    message(FATAL_ERROR "RNQNX_REANIMATED_SOURCE_DIR must be an installed react-native-reanimated package: ${pkg}")
  endif()
  file(READ "${pkg}/package.json" pkg_json)
  string(JSON pkg_name GET "${pkg_json}" name)
  string(JSON pkg_version GET "${pkg_json}" version)
  if(NOT pkg_name STREQUAL "react-native-reanimated" OR NOT pkg_version MATCHES "^4\\.7\\.[0-9]+$")
    message(FATAL_ERROR "react-native-reanimated 4.7.x is required (found ${pkg_name} ${pkg_version})")
  endif()
  set(common "${pkg}/Common/cpp")

  # --- text patches into generated copies --------------------------------------
  set(patched "${CMAKE_BINARY_DIR}/generated/reanimated/patched")
  file(READ "${common}/reanimated/Tools/PlatformDepMethodsHolder.h" holder)
  set(holder_from "#elif __APPLE__\nusing SynchronouslyUpdateUIPropsFunction")
  string(FIND "${holder}" "${holder_from}" holder_at)
  if(holder_at EQUAL -1)
    message(FATAL_ERROR "PlatformDepMethodsHolder.h changed: re-audit the QNX patch")
  endif()
  string(REPLACE "${holder_from}" "#else\nusing SynchronouslyUpdateUIPropsFunction" holder "${holder}")
  file(WRITE "${patched}/reanimated/Tools/PlatformDepMethodsHolder.h" "${holder}")

  file(READ "${common}/reanimated/NativeModules/ReanimatedModuleProxy.cpp" proxy)
  set(proxy_from "#if __APPLE__\n  for (const auto &[shadowNodeFamily, props] : synchronousUpdatesBatch) {")
  string(FIND "${proxy}" "${proxy_from}" proxy_at)
  if(proxy_at EQUAL -1)
    message(FATAL_ERROR "ReanimatedModuleProxy.cpp changed: re-audit the QNX patch")
  endif()
  string(REPLACE "${proxy_from}"
    "#if __APPLE__ || defined(__QNX__)\n  for (const auto &[shadowNodeFamily, props] : synchronousUpdatesBatch) {"
    proxy "${proxy}")
  file(WRITE "${patched}/reanimated/NativeModules/ReanimatedModuleProxy.cpp" "${proxy}")

  # --- Fabric codegen for the library's component specs ------------------------
  find_program(RNQNX_NODE node REQUIRED)
  set(gen "${CMAKE_BINARY_DIR}/generated/reanimated/fabric")
  execute_process(
    COMMAND "${RNQNX_NODE}" "${RNQNX_REACT_QT_ROOT}/scripts/fabric-codegen.mjs" rnreanimated "${gen}"
      "${pkg}/src/specs/SharedTransitionBoundaryNativeComponent.ts"
    WORKING_DIRECTORY "${RNQNX_REACT_QT_ROOT}"
    RESULT_VARIABLE codegen_result OUTPUT_VARIABLE codegen_output ERROR_VARIABLE codegen_error)
  if(NOT codegen_result EQUAL 0)
    message(FATAL_ERROR "Reanimated Fabric codegen failed: ${codegen_error}")
  endif()

  # --- sources -----------------------------------------------------------------
  file(GLOB_RECURSE upstream_sources CONFIGURE_DEPENDS "${common}/reanimated/*.cpp")
  list(FILTER upstream_sources EXCLUDE REGEX "/NativeModules/ReanimatedModuleProxy\\.cpp$")
  file(GLOB generated_sources "${gen}/react/renderer/components/rnreanimated/*.cpp")
  set(platform_dir "${RNQNX_REACT_QT_ROOT}/vnext/src/reanimated")
  add_library(rnqnx_reanimated STATIC
    ${upstream_sources}
    "${patched}/reanimated/NativeModules/ReanimatedModuleProxy.cpp"
    ${generated_sources}
    "${platform_dir}/QnxReanimatedModule.cpp"
    "${platform_dir}/QtReanimatedPlatform.cpp")
  set_source_files_properties(
    ${upstream_sources} "${patched}/reanimated/NativeModules/ReanimatedModuleProxy.cpp" ${generated_sources}
    PROPERTIES COMPILE_OPTIONS "-w")
  # The patched directory first, so its PlatformDepMethodsHolder.h wins.
  target_include_directories(rnqnx_reanimated
    PUBLIC "${RNQNX_REACT_QT_ROOT}/vnext/include"
    PRIVATE "${patched}" "${gen}" "${common}" "${platform_dir}")
  target_compile_definitions(rnqnx_reanimated PUBLIC RNQNX_ENABLE_REANIMATED=1
    PRIVATE REANIMATED_VERSION=${pkg_version} REACT_NATIVE_VERSION_MINOR=88)
  target_link_libraries(rnqnx_reanimated PUBLIC rnqnx_worklets rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_reanimated)
  message(STATUS "react-native-reanimated ${pkg_version} from ${pkg}")
endfunction()
