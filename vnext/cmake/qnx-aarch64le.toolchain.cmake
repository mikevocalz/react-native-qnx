# Canonical G0-A QNX OS 8 toolchain for Raspberry Pi 4.
#
# Source anchors:
# - QNX Everywhere Porting Guide, "CMake": CMAKE_SYSTEM_NAME QNX,
#   CMAKE_SYSTEM_PROCESSOR aarch64, q++, gcc_ntoaarch64le, QNX_TARGET sysroot.
# - QNX SDP 8 Utilities Reference, "q++, qcc": gcc_ntoaarch64le target.
#
# The installed target list still must be captured with qcc -V. This file only
# prepares the documented target; scripts/validate-g0-a.sh performs the gate.

set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if("$ENV{QNX_HOST}" STREQUAL "")
  message(FATAL_ERROR
    "QNX_HOST is empty. Source qnxsdp-env.sh from the installed QNX SDP 8.0 environment.")
endif()

if("$ENV{QNX_TARGET}" STREQUAL "")
  message(FATAL_ERROR
    "QNX_TARGET is empty. Source qnxsdp-env.sh from the installed QNX SDP 8.0 environment.")
endif()

file(TO_CMAKE_PATH "$ENV{QNX_HOST}" RNQNX_QNX_HOST)
file(TO_CMAKE_PATH "$ENV{QNX_TARGET}" RNQNX_QNX_TARGET)

set(RNQNX_QCC_TARGET "gcc_ntoaarch64le" CACHE STRING
  "Installed qcc target used for the QNX Pi 4 build")
if(NOT RNQNX_QCC_TARGET STREQUAL "gcc_ntoaarch64le")
  message(FATAL_ERROR
    "The qnx-pi4 preset requires gcc_ntoaarch64le; got ${RNQNX_QCC_TARGET}. Record qcc -V before changing this lock.")
endif()

set(CMAKE_C_COMPILER "${RNQNX_QNX_HOST}/usr/bin/qcc")
set(CMAKE_CXX_COMPILER "${RNQNX_QNX_HOST}/usr/bin/q++")
set(CMAKE_C_COMPILER_TARGET "${RNQNX_QCC_TARGET}")
set(CMAKE_CXX_COMPILER_TARGET "${RNQNX_QCC_TARGET}")
set(CMAKE_SYSROOT "${RNQNX_QNX_TARGET}")

if(NOT EXISTS "${CMAKE_C_COMPILER}")
  message(FATAL_ERROR "Installed qcc not found at ${CMAKE_C_COMPILER}")
endif()

if(NOT EXISTS "${CMAKE_CXX_COMPILER}")
  message(FATAL_ERROR "Installed q++ not found at ${CMAKE_CXX_COMPILER}")
endif()

set(CMAKE_FIND_ROOT_PATH "${RNQNX_QNX_TARGET}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
