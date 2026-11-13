# qnx-8.0.cmake plus a cross-built Qt prefix on the package search path.
# qnx-8.0.cmake restricts find_package to the SDP sysroot, which hides a Qt
# build tree; this adds the prefix named by RNQNX_QT_PREFIX (environment or
# cache variable). Used by scripts/webgpu/build-qt-compose.sh.
include("${CMAKE_CURRENT_LIST_DIR}/qnx-8.0.cmake")
if(NOT DEFINED RNQNX_QT_PREFIX AND DEFINED ENV{RNQNX_QT_PREFIX})
  set(RNQNX_QT_PREFIX "$ENV{RNQNX_QT_PREFIX}")
endif()
if(NOT RNQNX_QT_PREFIX)
  message(FATAL_ERROR "RNQNX_QT_PREFIX is not set (cross-built Qt prefix or build tree).")
endif()
list(APPEND CMAKE_FIND_ROOT_PATH "${RNQNX_QT_PREFIX}")
# Qt looks for its host tools (Qt6GuiTools and friends) under QT_HOST_PATH, which
# lies outside the sysroot, so packages are searched in both places.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
# AUTOMOC runs the compiler on its own to dump predefined macros and omits the
# target flags; QCC then has no target. Put the target on the compiler command.
set(CMAKE_C_COMPILER_ARG1 "-Vgcc_ntoaarch64le -D_QNX_SOURCE")
set(CMAKE_CXX_COMPILER_ARG1 "-Vgcc_ntoaarch64le -D_QNX_SOURCE")
