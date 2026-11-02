cmake_minimum_required(VERSION 3.31.9)

if(NOT DEFINED RNQNX_REACT_NATIVE_DIR)
  set(RNQNX_REACT_NATIVE_DIR "${CMAKE_SOURCE_DIR}/node_modules/react-native")
endif()

# Nitro v1 alpha code moves portable C++ runtime into nitro-core.
# This is a pinned release-source list, not a wildcard. Fail closed on drift.
set(RNQNX_NITRO_ROOT
  "${CMAKE_SOURCE_DIR}/node_modules/react-native-nitro-modules"
  CACHE PATH "Installed Nitro React Native platform package")
set(RNQNX_NITRO_CORE_ROOT
  "${CMAKE_SOURCE_DIR}/node_modules/nitro-core"
  CACHE PATH "Installed Nitro v1 C++ runtime package")
set(_core_src "${RNQNX_NITRO_CORE_ROOT}/native/cpp/src")
set(_core_inc "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include")

if(NOT EXISTS "${_core_inc}/NitroModules/HybridClassRegistry.hpp" OR
   NOT EXISTS "${_core_inc}/NitroModules/InstallNitro.hpp" OR
   NOT EXISTS "${RNQNX_NITRO_ROOT}/include/NitroModulesReactNative/NativeNitroModules.h")
  message(FATAL_ERROR
    "Nitro v1 core/platform headers are missing. Install exact reviewed "
    "Nitro/Nitrogen/nitro-core 1.x alpha releases; do not silently compile 0.37.x.")
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
    message(FATAL_ERROR "Pinned Nitro v1 C++ source missing: ${_source}")
  endif()
endforeach()

find_package(Threads REQUIRED)
add_library(rnqnx_nitro_qnx STATIC
  ${_nitro_sources}
  cpp/NitroLogger.cpp
  cpp/ThreadUtils.cpp
  cpp/QnxNitroRuntime.cpp
  cpp/QnxPlatformInfo.cpp
)

target_include_directories(rnqnx_nitro_qnx PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/cpp"
  "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi"
  "${_core_inc}"
  "${RNQNX_NITRO_ROOT}/include"
)
target_compile_definitions(rnqnx_nitro_qnx PUBLIC
  NITRO_VERSION="1.0.0-alpha.nightly.105.g69d365fc9b18"
)
target_link_libraries(rnqnx_nitro_qnx PUBLIC Threads::Threads)
target_compile_features(rnqnx_nitro_qnx PUBLIC cxx_std_20)
if(COMMAND rnqnx_enable_strict_warnings)
  rnqnx_enable_strict_warnings(rnqnx_nitro_qnx)
endif()
