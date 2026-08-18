# Hermes for QNX, built out-of-tree and imported as a prebuilt library.
#
# This module creates an `rnqnx_hermes` INTERFACE target that mirrors the host
# `rnqnx_hermes` from `Hermes.cmake`, but links the cross-compiled QNX artifacts
# instead of building from source. The headers are the Hermes public/API headers
# from the source tree; the JSI headers come from React Native's vendored copy
# (the same ones the host build uses) because that is the layout the probes and
# renderer include with `<jsi/jsi.h>`.

include_guard(GLOBAL)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
  message(FATAL_ERROR "HermesQNX.cmake is only for QNX cross-builds")
endif()

set(RNQNX_HERMES_QNX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/hermes-qnx-x86_64"
  CACHE PATH "Directory containing the QNX cross-built Hermes artifacts")

if(NOT IS_DIRECTORY "${RNQNX_HERMES_QNX_DIR}")
  message(FATAL_ERROR
    "QNX Hermes build tree not found: ${RNQNX_HERMES_QNX_DIR}\n"
    "Build it first (see docs/handoff/HERMES_QNX.md).")
endif()

set(_hermes_include "${CMAKE_CURRENT_SOURCE_DIR}/third-party/hermes-250829098/API")
set(_hermes_public_include "${CMAKE_CURRENT_SOURCE_DIR}/third-party/hermes-250829098/public")
set(_jsi_include "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi")

set(_hermes_so "${RNQNX_HERMES_QNX_DIR}/lib/libhermesvmlean.so")
set(_jsi_a "${RNQNX_HERMES_QNX_DIR}/jsi/libjsi.a")

foreach(_f IN ITEMS ${_hermes_so} ${_jsi_a})
  if(NOT EXISTS "${_f}")
    message(FATAL_ERROR "Missing QNX Hermes artifact: ${_f}")
  endif()
endforeach()

add_library(rnqnx_hermes INTERFACE)
target_link_libraries(rnqnx_hermes INTERFACE
  "${_hermes_so}"
  "${_jsi_a}"
)
target_include_directories(rnqnx_hermes SYSTEM INTERFACE
  "${_hermes_include}"
  "${_hermes_public_include}"
  "${_jsi_include}"
)
