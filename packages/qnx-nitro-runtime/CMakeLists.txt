cmake_minimum_required(VERSION 3.31.9)

if(NOT DEFINED RNQNX_REACT_NATIVE_DIR)
  set(RNQNX_REACT_NATIVE_DIR "${CMAKE_SOURCE_DIR}/node_modules/react-native")
endif()

set(RNQNX_NITRO_ROOT
  "${CMAKE_SOURCE_DIR}/node_modules/react-native-nitro-modules"
  CACHE PATH "Installed react-native-nitro-modules package")

if(NOT EXISTS "${RNQNX_NITRO_ROOT}/cpp/entrypoint/InstallNitro.cpp")
  message(FATAL_ERROR
    "Nitro shared C++ source not found at ${RNQNX_NITRO_ROOT}. "
    "Run pnpm install and keep react-native-nitro-modules on the audited 0.37.1 line.")
endif()

set(_nitro_cpp "${RNQNX_NITRO_ROOT}/cpp")
set(_nitro_sources
  "${_nitro_cpp}/core/AnyMap.cpp"
  "${_nitro_cpp}/core/ArrayBuffer.cpp"
  "${_nitro_cpp}/core/BoxedHybridObject.cpp"
  "${_nitro_cpp}/core/HybridObject.cpp"
  "${_nitro_cpp}/core/Promise.cpp"
  "${_nitro_cpp}/entrypoint/HybridNitroModulesProxy.cpp"
  "${_nitro_cpp}/entrypoint/InstallNitro.cpp"
  "${_nitro_cpp}/jsi/JSICache.cpp"
  "${_nitro_cpp}/prototype/HybridObjectPrototype.cpp"
  "${_nitro_cpp}/registry/HybridObjectRegistry.cpp"
  "${_nitro_cpp}/threading/Dispatcher.cpp"
  "${_nitro_cpp}/threading/ThreadPool.cpp"
  "${_nitro_cpp}/utils/CommonGlobals.cpp"
  "${_nitro_cpp}/utils/NitroTypeInfo.cpp"
  "${_nitro_cpp}/utils/PropNameIDCache.cpp"
)

foreach(_source IN LISTS _nitro_sources)
  if(NOT EXISTS "${_source}")
    message(FATAL_ERROR "Audited Nitro 0.37.1 source is missing: ${_source}")
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
  "${_nitro_cpp}/core"
  "${_nitro_cpp}/entrypoint"
  "${_nitro_cpp}/jsi"
  "${_nitro_cpp}/platform"
  "${_nitro_cpp}/prototype"
  "${_nitro_cpp}/registry"
  "${_nitro_cpp}/templates"
  "${_nitro_cpp}/threading"
  "${_nitro_cpp}/utils"
)

target_link_libraries(rnqnx_nitro_qnx PUBLIC Threads::Threads)
target_compile_features(rnqnx_nitro_qnx PUBLIC cxx_std_20)

if(COMMAND rnqnx_enable_strict_warnings)
  rnqnx_enable_strict_warnings(rnqnx_nitro_qnx)
endif()
