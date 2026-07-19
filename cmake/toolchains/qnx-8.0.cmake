# QNX SDP 8.0.5 CMake toolchain file
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
    "Install QNX SDP 8.0.5 and source the SDP environment script, e.g.:\n"
    "  source /opt/qnx/800/host_linux/x86_64/bin/qnxsdp-env.sh\n"
    "Then re-run CMake.")
endif()

if(NOT DEFINED ENV{QNX_TARGET})
  message(FATAL_ERROR
    "Environment variable QNX_TARGET is not set.\n"
    "Install QNX SDP 8.0.5 and source the SDP environment script, e.g.:\n"
    "  source /opt/qnx/800/host_linux/x86_64/bin/qnxsdp-env.sh\n"
    "Then re-run CMake.")
endif()

set(QNX_HOST "$ENV{QNX_HOST}")
set(QNX_TARGET "$ENV{QNX_TARGET}")

file(TO_CMAKE_PATH "${QNX_HOST}" QNX_HOST)
file(TO_CMAKE_PATH "${QNX_TARGET}" QNX_TARGET)

set(CMAKE_C_COMPILER   "${QNX_HOST}/usr/bin/qcc")
set(CMAKE_CXX_COMPILER "${QNX_HOST}/usr/bin/q++")

if(NOT EXISTS "${CMAKE_C_COMPILER}")
  message(FATAL_ERROR "QNX C compiler not found at ${CMAKE_C_COMPILER}. Verify QNX_HOST.")
endif()

if(NOT EXISTS "${CMAKE_CXX_COMPILER}")
  message(FATAL_ERROR "QNX C++ compiler not found at ${CMAKE_CXX_COMPILER}. Verify QNX_HOST.")
endif()

# qcc/q++ target variant flags. These map the toolchain to the target CPU.
if(QNX_TARGET_ARCH STREQUAL "aarch64")
  set(QNX_COMPILER_VARIANT "gcc_ntoaarch64le")
elseif(QNX_TARGET_ARCH STREQUAL "x86_64")
  set(QNX_COMPILER_VARIANT "gcc_ntox86_64")
endif()

set(QNX_TARGET_CFLAGS  "-V${QNX_COMPILER_VARIANT}")
set(QNX_TARGET_LDFLAGS "-V${QNX_COMPILER_VARIANT}")

set(CMAKE_C_COMPILER_TARGET   "${QNX_COMPILER_VARIANT}")
set(CMAKE_CXX_COMPILER_TARGET "${QNX_COMPILER_VARIANT}")

set(CMAKE_C_FLAGS   "${QNX_TARGET_CFLAGS}" CACHE STRING "Initial C compile flags" FORCE)
set(CMAKE_CXX_FLAGS "${QNX_TARGET_CFLAGS} -std=c++20" CACHE STRING "Initial C++ compile flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS    "${QNX_TARGET_LDFLAGS}" CACHE STRING "Initial linker flags" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${QNX_TARGET_LDFLAGS}" CACHE STRING "Initial shared linker flags" FORCE)
set(CMAKE_MODULE_LINKER_FLAGS "${QNX_TARGET_LDFLAGS}" CACHE STRING "Initial module linker flags" FORCE)

set(CMAKE_FIND_ROOT_PATH "${QNX_TARGET}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Feature probes -----------------------------------------------------------
include(CMakePushCheckState)
cmake_push_check_state()
set(CMAKE_REQUIRED_FLAGS "${QNX_TARGET_CFLAGS}")

include(CheckCXXSourceCompiles)

# C++20
set(CMAKE_REQUIRED_FLAGS "${QNX_TARGET_CFLAGS} -std=c++20")
check_cxx_source_compiles("
#include <version>
int main() {
#if __cplusplus < 202002L
#error C++20 required
#endif
  return 0;
}
" QNX_CXX20_SUPPORTED)
if(NOT QNX_CXX20_SUPPORTED)
  message(FATAL_ERROR "QNX compiler does not support C++20 (-std=c++20). Verify SDP installation.")
endif()

# Exceptions
check_cxx_source_compiles("
int main() { try { throw 0; } catch(int) { return 0; } return 1; }
" QNX_EXCEPTIONS_SUPPORTED)
if(NOT QNX_EXCEPTIONS_SUPPORTED)
  message(FATAL_ERROR "QNX compiler does not support C++ exceptions.")
endif()

# RTTI
check_cxx_source_compiles("
#include <typeinfo>
struct Base { virtual ~Base() = default; };
struct Derived : Base {};
int main() { Derived d; Base* b = &d; return typeid(*b) == typeid(Derived) ? 0 : 1; }
" QNX_RTTI_SUPPORTED)
if(NOT QNX_RTTI_SUPPORTED)
  message(FATAL_ERROR "QNX compiler does not support RTTI.")
endif()

# Atomics
check_cxx_source_compiles("
#include <atomic>
int main() { std::atomic<int> a{0}; return a.fetch_add(1); }
" QNX_ATOMICS_SUPPORTED)
if(NOT QNX_ATOMICS_SUPPORTED)
  message(FATAL_ERROR "QNX compiler does not support C++ atomics.")
endif()

# Thread-local storage
check_cxx_source_compiles("
thread_local int value = 42;
int main() { return value == 42 ? 0 : 1; }
" QNX_TLS_SUPPORTED)
if(NOT QNX_TLS_SUPPORTED)
  message(FATAL_ERROR "QNX compiler does not support thread_local storage.")
endif()

# Filesystem
check_cxx_source_compiles("
#include <filesystem>
int main() { return std::filesystem::path(\"/tmp\").empty() ? 1 : 0; }
" QNX_FILESYSTEM_SUPPORTED)
if(NOT QNX_FILESYSTEM_SUPPORTED)
  message(FATAL_ERROR "QNX compiler does not support std::filesystem.")
endif()

cmake_pop_check_state()

message(STATUS "QNX toolchain configured")
message(STATUS "  QNX_HOST:            ${QNX_HOST}")
message(STATUS "  QNX_TARGET:          ${QNX_TARGET}")
message(STATUS "  Target architecture: ${QNX_TARGET_ARCH}")
message(STATUS "  Compiler variant:    ${QNX_COMPILER_VARIANT}")
message(STATUS "  C compiler:          ${CMAKE_C_COMPILER}")
message(STATUS "  C++ compiler:        ${CMAKE_CXX_COMPILER}")
