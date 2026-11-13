include_guard(GLOBAL)

# react-native-worklets on Qt. Default OFF.
#
# The upstream C++ is not vendored. RNQNX_WORKLETS_SOURCE_DIR points at an
# installed react-native-worklets package (the one the app's JavaScript uses),
# and its Common/cpp tree is compiled here next to the QNX platform layer in
# vnext/src/worklets. The version must be the 0.13 line that Expo SDK 58 and
# React Native 0.88 select (see docs/worklets-qnx.md).

option(RNQNX_ENABLE_WORKLETS "Build react-native-worklets with the Qt UI-worklet platform layer" OFF)
set(RNQNX_WORKLETS_SOURCE_DIR "" CACHE PATH "Installed react-native-worklets package root")

function(rnqnx_add_worklets_target)
  if(TARGET rnqnx_worklets)
    return()
  endif()
  if(NOT TARGET rnqnx_react_host)
    message(FATAL_ERROR "RNQNX_ENABLE_WORKLETS needs the Qt/Hermes/Fabric host")
  endif()
  if(NOT RNQNX_WORKLETS_SOURCE_DIR)
    set(RNQNX_WORKLETS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/node_modules/react-native-worklets")
  endif()
  get_filename_component(pkg "${RNQNX_WORKLETS_SOURCE_DIR}" REALPATH)
  if(NOT EXISTS "${pkg}/package.json" OR NOT IS_DIRECTORY "${pkg}/Common/cpp/worklets")
    message(FATAL_ERROR "RNQNX_WORKLETS_SOURCE_DIR must be an installed react-native-worklets package: ${pkg}")
  endif()
  file(READ "${pkg}/package.json" pkg_json)
  string(JSON pkg_name GET "${pkg_json}" name)
  string(JSON pkg_version GET "${pkg_json}" version)
  if(NOT pkg_name STREQUAL "react-native-worklets" OR NOT pkg_version MATCHES "^0\\.13\\.[0-9]+$")
    message(FATAL_ERROR "react-native-worklets 0.13.x is required (found ${pkg_name} ${pkg_version})")
  endif()

  set(common "${pkg}/Common/cpp")
  file(GLOB_RECURSE upstream_sources CONFIGURE_DEPENDS "${common}/worklets/*.cpp")
  list(FILTER upstream_sources EXCLUDE REGEX "/RunLoop/AsyncQueueImpl\\.cpp$")
  list(FILTER upstream_sources EXCLUDE REGEX "/Tools/PlatformLogger\\.cpp$")

  # AsyncQueueImpl.cpp names its thread with the Apple form pthread_setname_np(name)
  # outside Android. QNX takes (thread, name). Patch one call in a generated copy
  # and fail loudly if upstream changes the line.
  file(READ "${common}/worklets/RunLoop/AsyncQueueImpl.cpp" async_queue)
  string(FIND "${async_queue}" "pthread_setname_np(name.c_str());" patch_at)
  if(patch_at EQUAL -1)
    message(FATAL_ERROR "AsyncQueueImpl.cpp changed: re-audit the QNX thread-name patch")
  endif()
  string(REPLACE "pthread_setname_np(name.c_str());" "pthread_setname_np(pthread_self(), name.c_str());"
    async_queue "${async_queue}")
  set(patched "${CMAKE_BINARY_DIR}/generated/worklets/AsyncQueueImpl.qnx.cpp")
  file(WRITE "${patched}" "${async_queue}")

  set(platform_dir "${RNQNX_REACT_QT_ROOT}/vnext/src/worklets")
  add_library(rnqnx_worklets STATIC
    ${upstream_sources}
    "${patched}"
    "${platform_dir}/QnxWorkletsPlatformLogger.cpp"
    "${platform_dir}/QnxWorkletsModule.cpp"
    "${platform_dir}/QtFrameBridge.cpp"
    "${platform_dir}/QtUiThread.cpp"
    "${platform_dir}/QtWorkletsPlatform.cpp"
  )
  # Upstream stays exactly as published; only our platform files get strict flags.
  set_source_files_properties(${upstream_sources} "${patched}" PROPERTIES COMPILE_OPTIONS "-w")
  target_include_directories(rnqnx_worklets
    PUBLIC "${RNQNX_REACT_QT_ROOT}/vnext/include"
    PRIVATE "${common}" "${platform_dir}")
  target_compile_definitions(rnqnx_worklets PUBLIC
    RNQNX_ENABLE_WORKLETS=1
    PRIVATE WORKLETS_VERSION=${pkg_version})
  target_link_libraries(rnqnx_worklets PUBLIC rnqnx_react_host Qt6::Quick Qt6::Gui)
  rnqnx_enable_strict_warnings(rnqnx_worklets)
  message(STATUS "react-native-worklets ${pkg_version} from ${pkg}")
endfunction()
