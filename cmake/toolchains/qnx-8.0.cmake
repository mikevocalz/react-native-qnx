# Generic QNX SDP 8.0 CMake toolchain file
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-8.0.cmake \
#         -DQNX_TARGET_ARCH=aarch64|x86_64 \
#         -DCMAKE_BUILD_TYPE=Debug|Release|RelWithDebInfo \
#         -S . -B build/qnx-<arch>-<config>
#
# Required environment variables:
#   QNX_HOST   - path to the host toolchain directory
#   QNX_TARGET - path to the target sysroot

set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0)

if(NOT DEFINED QNX_TARGET_ARCH)
  set(QNX_TARGET_ARCH "aarch64" CACHE STRING "Target architecture: aarch64 or x86_64")
else()
  set(QNX_TARGET_ARCH "${QNX_TARGET_ARCH}" CACHE STRING "Target architecture: aarch64 or x86_64")
endif()

if(NOT QNX_TARGET_ARCH STREQUAL "aarch64" AND NOT QNX_TARGET_ARCH STREQUAL "x86_64")
  message(FATAL_ERROR "QNX_TARGET_ARCH must be 'aarch64' or 'x86_64', got '${QNX_TARGET_ARCH}'")
endif()

set(CMAKE_SYSTEM_PROCESSOR "${QNX_TARGET_ARCH}")

if(NOT DEFINED ENV{QNX_HOST})
  message(FATAL_ERROR
    "Environment variable QNX_HOST is not set.\n"
    "Install QNX SDP 8.0 and source the qnxsdp-env.sh script from that installation.\n"
    "Then re-run CMake.")
endif()

if(NOT DEFINED ENV{QNX_TARGET})
  message(FATAL_ERROR
    "Environment variable QNX_TARGET is not set.\n"
    "Install QNX SDP 8.0 and source the qnxsdp-env.sh script from that installation.\n"
    "Then re-run CMake.")
endif()

set(QNX_HOST "$ENV{QNX_HOST}")
set(QNX_TARGET "$ENV{QNX_TARGET}")

file(TO_CMAKE_PATH "${QNX_HOST}" QNX_HOST)
file(TO_CMAKE_PATH "${QNX_TARGET}" QNX_TARGET)

# Locate the QNX wrapper compilers.  On Windows the host tools have a .exe
# extension, so find_program is used instead of a hard-coded path/existence test.
find_program(CMAKE_C_COMPILER
  NAMES qcc qcc.exe
  HINTS "${QNX_HOST}/usr/bin"
  NO_DEFAULT_PATH
  REQUIRED
)
find_program(CMAKE_CXX_COMPILER
  NAMES q++ q++.exe
  HINTS "${QNX_HOST}/usr/bin"
  NO_DEFAULT_PATH
  REQUIRED
)

# qcc/q++ target variant flags. These map the wrapper to the target CPU.
if(QNX_TARGET_ARCH STREQUAL "aarch64")
  set(QNX_COMPILER_VARIANT "gcc_ntoaarch64le")
  set(RNQNX_QNX_TOOL_PREFIX "aarch64-unknown-nto-qnx8.0.0")
elseif(QNX_TARGET_ARCH STREQUAL "x86_64")
  set(QNX_COMPILER_VARIANT "gcc_ntox86_64")
  set(RNQNX_QNX_TOOL_PREFIX "x86_64-pc-nto-qnx8.0.0")
endif()

set(QNX_TARGET_CFLAGS  "-V${QNX_COMPILER_VARIANT} -D_QNX_SOURCE")
set(QNX_TARGET_LDFLAGS "-V${QNX_COMPILER_VARIANT}")

# Hermes' bundled llvh expects <machine/endian.h>, which QNX does not ship in
# the public sysroot. Provide a shim directory that sits next to this toolchain.
get_filename_component(_QNX_TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
set(QNX_TARGET_ARCH_INCLUDES
    "-isystem ${_QNX_TOOLCHAIN_DIR}/qnx-hermes-shims")

set(CMAKE_C_FLAGS   "${QNX_TARGET_CFLAGS} ${QNX_TARGET_ARCH_INCLUDES}" CACHE STRING "Initial C compile flags" FORCE)
set(CMAKE_CXX_FLAGS "${QNX_TARGET_CFLAGS} -std=c++20 ${QNX_TARGET_ARCH_INCLUDES}" CACHE STRING "Initial C++ compile flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS    "${QNX_TARGET_LDFLAGS}" CACHE STRING "Initial linker flags" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${QNX_TARGET_LDFLAGS}" CACHE STRING "Initial shared linker flags" FORCE)
set(CMAKE_MODULE_LINKER_FLAGS "${QNX_TARGET_LDFLAGS}" CACHE STRING "Initial module linker flags" FORCE)

# Static archives need the cross-target archiver/ranlib. CMake does not infer
# these from the compiler wrapper, so locate them explicitly.
find_program(CMAKE_AR
  NAMES ${RNQNX_QNX_TOOL_PREFIX}-ar ${RNQNX_QNX_TOOL_PREFIX}-ar.exe
  HINTS "${QNX_HOST}/usr/bin"
  NO_DEFAULT_PATH
  REQUIRED
)
find_program(CMAKE_RANLIB
  NAMES ${RNQNX_QNX_TOOL_PREFIX}-ranlib ${RNQNX_QNX_TOOL_PREFIX}-ranlib.exe
  HINTS "${QNX_HOST}/usr/bin"
  NO_DEFAULT_PATH
  REQUIRED
)

set(CMAKE_FIND_ROOT_PATH "${QNX_TARGET}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

message(STATUS "QNX toolchain configured")
message(STATUS "  QNX_HOST:            ${QNX_HOST}")
message(STATUS "  QNX_TARGET:          ${QNX_TARGET}")
message(STATUS "  Target architecture: ${QNX_TARGET_ARCH}")
message(STATUS "  Compiler variant:    ${QNX_COMPILER_VARIANT}")
message(STATUS "  C compiler:          ${CMAKE_C_COMPILER}")
message(STATUS "  C++ compiler:        ${CMAKE_CXX_COMPILER}")
