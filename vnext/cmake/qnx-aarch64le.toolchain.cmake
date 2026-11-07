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

# On Windows the QNX host tools ship as .exe; find_program tolerates the
# extension while preserving the explicit QNX host path.
find_program(CMAKE_C_COMPILER
  NAMES qcc qcc.exe
  HINTS "${RNQNX_QNX_HOST}/usr/bin"
  NO_DEFAULT_PATH
  REQUIRED
)
find_program(CMAKE_CXX_COMPILER
  NAMES q++ q++.exe
  HINTS "${RNQNX_QNX_HOST}/usr/bin"
  NO_DEFAULT_PATH
  REQUIRED
)

find_program(CMAKE_AR
  NAMES aarch64-unknown-nto-qnx8.0.0-ar aarch64-unknown-nto-qnx8.0.0-ar.exe
  HINTS "${RNQNX_QNX_HOST}/usr/bin"
  NO_DEFAULT_PATH
  REQUIRED
)

find_program(CMAKE_RANLIB
  NAMES aarch64-unknown-nto-qnx8.0.0-ranlib aarch64-unknown-nto-qnx8.0.0-ranlib.exe
  HINTS "${RNQNX_QNX_HOST}/usr/bin"
  NO_DEFAULT_PATH
  REQUIRED
)

set(CMAKE_SYSROOT "${RNQNX_QNX_TARGET}")

# CMake's QCC MOC predefinitions command includes COMPILER_ARG1 but omits
# CMAKE_CXX_FLAGS and COMPILER_TARGET. Keep its architecture and native header
# feature surface identical to ordinary compilation.
set(CMAKE_C_COMPILER_ARG1 "-V${RNQNX_QCC_TARGET} -D_QNX_SOURCE")
set(CMAKE_CXX_COMPILER_ARG1 "-V${RNQNX_QCC_TARGET} -D_QNX_SOURCE")

# qcc/q++ select the OS/CPU variant with -V.  The wrapper ignores CMake's
# generic compiler-target property, so pass the variant through the flags.
# Strict language modes otherwise select a smaller sched_param layout in the
# QNX headers, changing pthread_attr_t's ABI relative to the target libc.
set(CMAKE_C_FLAGS "-V${RNQNX_QCC_TARGET} -D_QNX_SOURCE" CACHE STRING "Initial C compile flags" FORCE)
set(CMAKE_CXX_FLAGS "-V${RNQNX_QCC_TARGET} -D_QNX_SOURCE" CACHE STRING "Initial C++ compile flags" FORCE)

set(CMAKE_FIND_ROOT_PATH "${RNQNX_QNX_TARGET}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
