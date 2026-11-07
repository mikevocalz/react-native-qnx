include_guard(GLOBAL)

# Single source-of-truth for the portable Nitro v1 C++ ABI shared by the
# separately compiled QNX and MarathonOS platform adapters. This target has
# no QNX Screen, Wayland compositor, Android, or WebView dependency.
get_filename_component(RNQNX_NITRO_V1_REPO_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(RNQNX_NITRO_V1_COMPAT
  "${RNQNX_NITRO_V1_REPO_ROOT}/packages/qnx-nitro-runtime/cpp/NitroCoreCompat.hpp")
# The global include guard is shared by QNX and Marathon subdirectories, so
# freeze these paths in the internal CMake cache for both adapter scopes.
set(RNQNX_NITRO_V1_REPO_ROOT "${RNQNX_NITRO_V1_REPO_ROOT}" CACHE INTERNAL
  "Portable Nitro v1 shared repository root" FORCE)
set(RNQNX_NITRO_V1_COMPAT "${RNQNX_NITRO_V1_COMPAT}" CACHE INTERNAL
  "Portable Nitro v1 compatibility include" FORCE)

function(rnqnx_add_nitro_v1_core)
  if(TARGET rnqnx_nitro_core)
    return()
  endif()

  if(NOT DEFINED RNQNX_REACT_NATIVE_DIR)
    set(RNQNX_REACT_NATIVE_DIR "${RNQNX_NITRO_V1_REPO_ROOT}/node_modules/react-native")
  endif()
  if(NOT DEFINED RNQNX_NITRO_ROOT)
    set(RNQNX_NITRO_ROOT "${RNQNX_NITRO_V1_REPO_ROOT}/node_modules/react-native-nitro-modules")
  endif()
  if(NOT DEFINED RNQNX_NITRO_CORE_ROOT)
    set(RNQNX_NITRO_CORE_ROOT "${RNQNX_NITRO_V1_REPO_ROOT}/node_modules/nitro-core")
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dependencies/QnxNitroPatches.cmake)
  endif()

  set(_core_src "${RNQNX_NITRO_CORE_ROOT}/native/cpp/src")
  set(_core_inc "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include")
  if(NOT EXISTS "${_core_inc}/NitroModules/HybridClassRegistry.hpp" OR
     NOT EXISTS "${_core_inc}/NitroModules/InstallNitro.hpp" OR
     NOT EXISTS "${RNQNX_NITRO_ROOT}/include/NitroModulesReactNative/NativeNitroModules.h")
    message(FATAL_ERROR "The audited Nitro v1 native core/platform headers are missing")
  endif()

  set(_nitro_sources
    "${_core_src}/ArrayBuffer.cpp"
    "${_core_src}/BoxedHybridObject.cpp"
    "${_core_src}/CommonGlobals.cpp"
    "${_core_src}/Dispatcher.cpp"
    "${_core_src}/GlobalErrorHandler.cpp"
    "${_core_src}/HybridClass.cpp"
    "${_core_src}/HybridClassRegistry.cpp"
    "${_core_src}/HybridNitroModulesProxy.cpp"
    "${_core_src}/HybridObject.cpp"
    "${_core_src}/InstallNitro.cpp"
    "${_core_src}/JSICache.cpp"
    "${_core_src}/JSIConverter+Promise.cpp"
    "${_core_src}/NitroTypeInfo.cpp"
    "${_core_src}/Promise.cpp"
    "${_core_src}/PropNameIDCache.cpp"
    "${_core_src}/RoundTripResult.cpp"
    "${_core_src}/RuntimeData.cpp"
    "${_core_src}/ThreadPool.cpp"
  )
  foreach(_source IN LISTS _nitro_sources)
    if(NOT EXISTS "${_source}")
      message(FATAL_ERROR "Pinned Nitro v1 native source is missing: ${_source}")
    endif()
  endforeach()

  find_package(Threads REQUIRED)
  add_library(rnqnx_nitro_core STATIC
    ${_nitro_sources}
    "${RNQNX_NITRO_V1_REPO_ROOT}/packages/qnx-nitro-runtime/cpp/NitroLogger.cpp"
    "${RNQNX_NITRO_V1_REPO_ROOT}/packages/qnx-nitro-runtime/cpp/ThreadUtils.cpp"
  )
  target_include_directories(rnqnx_nitro_core PUBLIC
    "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi"
    "${_core_inc}"
    "${RNQNX_NITRO_ROOT}/include"
  )
  target_compile_definitions(rnqnx_nitro_core PUBLIC
    NITRO_VERSION=\"1.0.0-alpha.nightly.105.g69d365fc9b18\"
  )
  target_link_libraries(rnqnx_nitro_core PUBLIC Threads::Threads)
  target_compile_features(rnqnx_nitro_core PUBLIC cxx_std_20)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|QCC")
    message(FATAL_ERROR "Nitro 1.0 alpha compatibility preinclude requires Clang/GCC")
  endif()
  target_compile_options(rnqnx_nitro_core PRIVATE
    "-include" "${RNQNX_NITRO_V1_COMPAT}")
  if(COMMAND rnqnx_enable_strict_warnings)
    rnqnx_enable_strict_warnings(rnqnx_nitro_core)
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # After -Wpedantic: Nitro intentionally uses Swift nullability annotations.
    target_compile_options(rnqnx_nitro_core PUBLIC -Wno-nullability-extension)
  endif()
endfunction()

# The QNX and Marathon adapters compile as distinct libraries and consume the
# same portable Nitro core, but only their own OS-specific class/installer.
function(rnqnx_configure_nitro_v1_adapter target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "Unknown Nitro v1 adapter: ${target}")
  endif()
  if(NOT TARGET rnqnx_nitro_core)
    message(FATAL_ERROR "Portable Nitro core must exist before adapter ${target}")
  endif()
  target_link_libraries(${target} PUBLIC rnqnx_nitro_core)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_compile_options(${target} PRIVATE
    "-include" "${RNQNX_NITRO_V1_COMPAT}")
  if(COMMAND rnqnx_enable_strict_warnings)
    rnqnx_enable_strict_warnings(${target})
  endif()
endfunction()
