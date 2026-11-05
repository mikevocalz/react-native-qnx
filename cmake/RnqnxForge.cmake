include_guard(GLOBAL)

set(RNQNX_FORGE_SOURCE_DIR "" CACHE PATH "Absolute path to the reviewed nitro-forge checkout")
set(RNQNX_FORGE_RUST_TARGET "" CACHE STRING "Rust target triple used to build the QNX nf-sys archive")
option(RNQNX_FORGE_BUILD_PROBE "Build Forge's shared native write/reopen probe" OFF)

function(rnqnx_add_forge_runtime mode)
  if(mode STREQUAL "QNX")
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
      message(FATAL_ERROR "RNQNX_BUILD_FORGE_QNX requires a QNX toolchain. Use tests/native/forge-host for host evidence.")
    endif()
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
      set(expected_target "aarch64-unknown-qnx")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
      set(expected_target "x86_64-pc-qnx")
    else()
      message(FATAL_ERROR "Forge QNX supports only the reviewed aarch64 and x86_64 toolchain configurations")
    endif()
    if(NOT RNQNX_FORGE_RUST_TARGET STREQUAL expected_target)
      message(FATAL_ERROR "RNQNX_FORGE_RUST_TARGET must be ${expected_target}; do not link a host nf-sys archive into QNX.")
    endif()
    get_filename_component(compiler_name "${CMAKE_CXX_COMPILER}" NAME)
    if(NOT compiler_name MATCHES "^(q\\+\\+|qcc)(\\.exe)?$" OR
       NOT IS_DIRECTORY "$ENV{QNX_HOST}" OR NOT IS_DIRECTORY "$ENV{QNX_TARGET}")
      message(FATAL_ERROR "Forge QNX requires the licensed QNX q++/qcc compiler and sourced QNX_HOST/QNX_TARGET environment.")
    endif()
  elseif(mode STREQUAL "HOST_TEST")
    if(CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
      message(FATAL_ERROR "Forge host tests require a native Linux or macOS build; they are not QNX target evidence.")
    endif()
  else()
    message(FATAL_ERROR "rnqnx_add_forge_runtime expects QNX or HOST_TEST")
  endif()

  if(NOT IS_ABSOLUTE "${RNQNX_FORGE_SOURCE_DIR}")
    message(FATAL_ERROR "RNQNX_FORGE_SOURCE_DIR must be an absolute path to a reviewed nitro-forge checkout.")
  endif()
  foreach(required_file
      packages/ignition-native/CMakeLists.txt
      packages/ignition-v1-lab/cpp/ForgeRustSession.hpp
      packages/ignition-v1-lab/cpp/ForgeSerialQueue.hpp
      rust/nf-sys/include/nf_v1.h)
    if(NOT EXISTS "${RNQNX_FORGE_SOURCE_DIR}/${required_file}")
      message(FATAL_ERROR "RNQNX_FORGE_SOURCE_DIR is missing ${required_file}; use the documented Forge revision.")
    endif()
  endforeach()

  if(RNQNX_FORGE_BUILD_PROBE)
    set(NF_BUILD_TESTS ON CACHE BOOL "Build standalone Forge native integration tests" FORCE)
  endif()
  # The shared package validates NF_RUST_LIBRARY and supplies target system
  # libraries. It never invokes Cargo or substitutes a host archive.
  add_subdirectory("${RNQNX_FORGE_SOURCE_DIR}/packages/ignition-native"
    "${CMAKE_CURRENT_BINARY_DIR}/forge-native")

  get_filename_component(adapter_dir
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../native/platform/qnx/forge" ABSOLUTE)
  add_library(rnqnx_forge_runtime STATIC
    "${adapter_dir}/ForgeWorker.cpp"
    "${adapter_dir}/QnxForgeRuntime.cpp"
    "${adapter_dir}/QnxForgeDocument.cpp")
  target_include_directories(rnqnx_forge_runtime PUBLIC "${adapter_dir}")
  target_compile_features(rnqnx_forge_runtime PUBLIC cxx_std_20)
  target_link_libraries(rnqnx_forge_runtime PUBLIC forge_rust_session forge_serial_queue)
  set_target_properties(rnqnx_forge_runtime PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
  target_compile_options(rnqnx_forge_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror)
endfunction()
