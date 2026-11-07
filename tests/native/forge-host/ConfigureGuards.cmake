# These are intentional configuration failures, not QNX cross-build evidence.
file(MAKE_DIRECTORY "${GUARD_BINARY_DIR}/source")
file(WRITE "${GUARD_BINARY_DIR}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.31.9)
project(ForgeGuard LANGUAGES NONE)
include("${RNQNX_REPO_ROOT}/cmake/RnqnxForge.cmake")
if(GUARD_CASE STREQUAL "wrong-os")
  set(CMAKE_SYSTEM_NAME Linux)
  rnqnx_add_forge_runtime(QNX)
elseif(GUARD_CASE STREQUAL "wrong-target")
  set(CMAKE_SYSTEM_NAME QNX)
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
  set(RNQNX_FORGE_RUST_TARGET x86_64-unknown-linux-gnu)
  rnqnx_add_forge_runtime(QNX)
elseif(GUARD_CASE STREQUAL "missing-sdk")
  set(CMAKE_SYSTEM_NAME QNX)
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
  set(RNQNX_FORGE_RUST_TARGET aarch64-unknown-qnx)
  set(CMAKE_CXX_COMPILER /unavailable/c++)
  rnqnx_add_forge_runtime(QNX)
elseif(GUARD_CASE STREQUAL "missing-source")
  set(CMAKE_SYSTEM_NAME Linux)
  set(RNQNX_FORGE_SOURCE_DIR /unavailable/forge)
  rnqnx_add_forge_runtime(HOST_TEST)
elseif(GUARD_CASE STREQUAL "missing-archive")
  set(CMAKE_SYSTEM_NAME Linux)
  set(NF_RUST_LIBRARY /unavailable/libnf_sys.a CACHE FILEPATH "" FORCE)
  rnqnx_add_forge_runtime(HOST_TEST)
endif()
]=])

set(cases wrong-os wrong-target missing-sdk missing-source missing-archive)
set(messages "requires a QNX toolchain" "RNQNX_FORGE_RUST_TARGET must be"
  "requires the licensed QNX" "RNQNX_FORGE_SOURCE_DIR is missing"
  "NF_RUST_LIBRARY must name")
foreach(index RANGE 0 4)
  list(GET cases ${index} case)
  list(GET messages ${index} expected)
  execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${GUARD_BINARY_DIR}/source" -B "${GUARD_BINARY_DIR}/${case}"
    "-DRNQNX_REPO_ROOT=${RNQNX_REPO_ROOT}"
    "-DRNQNX_FORGE_SOURCE_DIR=${RNQNX_FORGE_SOURCE_DIR}"
    "-DGUARD_CASE=${case}"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  if(result EQUAL 0 OR NOT "${stdout}${stderr}" MATCHES "${expected}")
    message(FATAL_ERROR "Guard ${case} failed to reject with '${expected}':\n${stdout}\n${stderr}")
  endif()
  message(STATUS "PASS Forge configure guard: ${case}")
endforeach()
