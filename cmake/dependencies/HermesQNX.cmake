# Import an out-of-tree QNX build of the exact Hermes source selected by the
# current React Native package.

include_guard(GLOBAL)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
  message(FATAL_ERROR "HermesQNX.cmake is only for QNX cross-builds")
endif()

set(RNQNX_HERMES_QNX_TAG "hermes-v260318099.0.5"
  CACHE STRING "Hermes source tag audited for QNX")
set(RNQNX_HERMES_QNX_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/third-party/hermes-260318099.0.5"
  CACHE PATH "Patched Hermes source tree for QNX")
set(RNQNX_HERMES_QNX_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/build/hermes-qnx-${QNX_TARGET_ARCH}"
  CACHE PATH "QNX Hermes build tree")

if(NOT IS_DIRECTORY "${RNQNX_HERMES_QNX_SOURCE_DIR}")
  message(FATAL_ERROR
    "QNX Hermes source tree not found: ${RNQNX_HERMES_QNX_SOURCE_DIR}\n"
    "Run scripts/prepare-hermes-qnx.sh first.")
endif()
if(NOT IS_DIRECTORY "${RNQNX_HERMES_QNX_DIR}")
  message(FATAL_ERROR
    "QNX Hermes build tree not found: ${RNQNX_HERMES_QNX_DIR}\n"
    "Run scripts/configure-hermes-qnx.sh ${QNX_TARGET_ARCH} first.")
endif()

set(_rnqnx_hermes_tag_file "${RNQNX_HERMES_QNX_SOURCE_DIR}/.rnqnx-source-tag")
if(NOT EXISTS "${_rnqnx_hermes_tag_file}")
  message(FATAL_ERROR
    "Prepared Hermes source is missing ${_rnqnx_hermes_tag_file}")
endif()
file(READ "${_rnqnx_hermes_tag_file}" _rnqnx_hermes_source_tag)
string(STRIP "${_rnqnx_hermes_source_tag}" _rnqnx_hermes_source_tag)
if(NOT _rnqnx_hermes_source_tag STREQUAL RNQNX_HERMES_QNX_TAG)
  message(FATAL_ERROR
    "Prepared Hermes source is ${_rnqnx_hermes_source_tag}, expected "
    "${RNQNX_HERMES_QNX_TAG}.")
endif()

set(_hermes_include "${RNQNX_HERMES_QNX_SOURCE_DIR}/API")
set(_hermes_public_include "${RNQNX_HERMES_QNX_SOURCE_DIR}/public")
set(_jsi_include "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi")
# Release apps run precompiled bytecode on the lean VM. The development client
# evaluates Metro's JavaScript and every Fast Refresh update, which needs the
# VM with the compiler (build target hermesvm in the same Hermes build tree).
if(RNQNX_ENABLE_DEV_SUPPORT)
  set(_hermes_so "${RNQNX_HERMES_QNX_DIR}/lib/libhermesvm.so")
else()
  set(_hermes_so "${RNQNX_HERMES_QNX_DIR}/lib/libhermesvmlean.so")
endif()
set(_jsi_a "${RNQNX_HERMES_QNX_DIR}/jsi/libjsi.a")

foreach(_f IN ITEMS "${_hermes_so}" "${_jsi_a}")
  if(NOT EXISTS "${_f}")
    message(FATAL_ERROR "Missing QNX Hermes artifact: ${_f}")
  endif()
endforeach()

# React Native's renderer CMake expects a target named `jsi`. The QNX Hermes
# build produces that exact implementation as libjsi.a out-of-tree, so expose it
# as the canonical target instead of compiling a second JSI copy.
if(NOT TARGET jsi)
  add_library(jsi INTERFACE)
  target_link_libraries(jsi INTERFACE "${_jsi_a}")
  target_include_directories(jsi SYSTEM INTERFACE "${_jsi_include}")
endif()

add_library(rnqnx_hermes INTERFACE)
target_link_libraries(rnqnx_hermes INTERFACE
  "${_hermes_so}"
  jsi
)
target_include_directories(rnqnx_hermes SYSTEM INTERFACE
  "${_hermes_include}"
  "${_hermes_public_include}"
  "${_jsi_include}"
)
