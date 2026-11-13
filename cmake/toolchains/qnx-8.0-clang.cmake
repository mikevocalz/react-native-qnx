# Cross-compiles for QNX SDP 8.0 aarch64le with LLVM clang instead of the SDP's
# gcc. The SDP's compilers are x86-64 Linux binaries (they run under Rosetta in
# the Apple Silicon build VM and compile slowly); clang on any host can target
# the SDP's sysroot directly. Only compilation and archiving happen here: the
# SDP's linker is a Linux binary, so link executables in the qnx805-build
# container (scripts/webgpu/build-testapp.sh does that).
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-8.0-clang.cmake \
#         -DRNQNX_LLVM_BIN=/opt/homebrew/opt/llvm/bin \
#         -DRNQNX_QNX_TARGET=<sdp>/target/qnx ...
#
# RNQNX_LLVM_BIN needs clang, clang++, llvm-ar and llvm-ranlib (LLVM 19 or newer).
# Not a replacement for qnx-8.0.cmake: that file stays the toolchain for
# everything that links.
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

foreach(_var RNQNX_LLVM_BIN RNQNX_QNX_TARGET)
  if(NOT DEFINED ${_var} AND DEFINED ENV{${_var}})
    set(${_var} "$ENV{${_var}}")
  endif()
  if(NOT ${_var})
    message(FATAL_ERROR "${_var} is not set (cache variable or environment).")
  endif()
endforeach()

set(CMAKE_C_COMPILER "${RNQNX_LLVM_BIN}/clang")
set(CMAKE_CXX_COMPILER "${RNQNX_LLVM_BIN}/clang++")
set(CMAKE_AR "${RNQNX_LLVM_BIN}/llvm-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${RNQNX_LLVM_BIN}/llvm-ranlib" CACHE FILEPATH "")

# clang has no QNX operating-system triple, so the target is bare AArch64 ELF and
# the macros that the SDP's gcc driver predefines are supplied by hand (taken
# from `qcc -Vgcc_ntoaarch64le -dM -E`). -DQNX_LIBM_BUILTINS makes libc++'s math
# wrappers use compiler builtins, which clang needs for isinf/isnan.
set(_libcxx "${RNQNX_QNX_TARGET}/usr/include/c++/v1")
set(_common "--target=aarch64-unknown-none-elf -fPIC -fuse-cxa-atexit -fgnuc-version=12.2.0")
string(APPEND _common " --sysroot=${RNQNX_QNX_TARGET}")
string(APPEND _common " -D__QNX__=800 -D__QNXNTO__ -D__LITTLEENDIAN__ -D__unix__ -D__unix")
string(APPEND _common " -D_QNX_SOURCE -DQNX_LIBM_BUILTINS")
# The SDP's libc++.so was built by gcc and does not export every member that
# the headers' extern templates promise (for example basic_ostringstream::str()
# const&), so clang objects would fail to link. Without the extern templates
# clang instantiates those members itself.
string(APPEND _common " -D_LIBCPP_DISABLE_EXTERN_TEMPLATE")
# The bare-metal triple does not add the sysroot's include directory by itself.
set(_sysinc "-isystem ${RNQNX_QNX_TARGET}/usr/include")
set(CMAKE_C_FLAGS_INIT "${_common} ${_sysinc}")
# The SDP ships libc++; it has to come before the C headers, and clang must not
# look for a host C++ library.
set(CMAKE_CXX_FLAGS_INIT "${_common} -nostdinc++ -isystem ${_libcxx} ${_sysinc}")

# Nothing can be linked on the build host, so configure checks stop at objects.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH "${RNQNX_QNX_TARGET}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
