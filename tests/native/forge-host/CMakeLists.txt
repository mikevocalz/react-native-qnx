cmake_minimum_required(VERSION 3.31.9)
project(rnqnx_forge_host LANGUAGES CXX)

enable_testing()
option(RNQNX_ENABLE_HOST_SANITIZERS "Instrument the C++ boundary with ASan and UBSan" ON)
get_filename_component(RNQNX_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${RNQNX_REPO_ROOT}/cmake/RnqnxForge.cmake")
set(NF_BUILD_TESTS ON CACHE BOOL "Build standalone Forge native integration tests" FORCE)
rnqnx_add_forge_runtime(HOST_TEST)

add_executable(rnqnx_forge_host_test QnxForgeRuntimeTest.cpp)
target_link_libraries(rnqnx_forge_host_test PRIVATE rnqnx_forge_runtime)
target_compile_options(rnqnx_forge_host_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(RNQNX_ENABLE_HOST_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    message(FATAL_ERROR "Forge host sanitizer flags require GCC or Clang")
  endif()
  foreach(target rnqnx_forge_runtime forge_rust_session forge_serial_queue
      rnqnx_forge_host_test forge_shared_engine_roundtrip forge_serial_queue_test)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  endforeach()
endif()

add_test(NAME rnqnx_forge_host_lifecycle COMMAND rnqnx_forge_host_test)
set_tests_properties(rnqnx_forge_host_lifecycle PROPERTIES TIMEOUT 30)
add_test(NAME rnqnx_forge_configure_guards COMMAND "${CMAKE_COMMAND}"
  "-DRNQNX_REPO_ROOT=${RNQNX_REPO_ROOT}"
  "-DRNQNX_FORGE_SOURCE_DIR=${RNQNX_FORGE_SOURCE_DIR}"
  "-DGUARD_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/configure-guards"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/ConfigureGuards.cmake")
set_tests_properties(rnqnx_forge_configure_guards PROPERTIES TIMEOUT 30)
