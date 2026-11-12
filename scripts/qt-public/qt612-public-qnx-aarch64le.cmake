# Same as the evaluation's qt612-qnx-aarch64le.cmake, pointed at the public source tree.
include(/work/qt-6.12.0-public/src/qt-everywhere-src-6.12.0/coin/provisioning/common/shared/cmake_toolchain_files/qnx-toolchain-aarch64le.cmake)
set(CMAKE_SYSTEM_VERSION 800)
# QNX target pthread ABI requires the native feature surface.
set(CMAKE_C_FLAGS_INIT "-Vgcc_ntoaarch64le -D_QNX_SOURCE")
set(CMAKE_CXX_FLAGS_INIT "-Vgcc_ntoaarch64le -D_QNX_SOURCE")
set(CMAKE_RANLIB $ENV{QNX_HOST}/usr/bin/ntoaarch64-ranlib)
set(CMAKE_NM $ENV{QNX_HOST}/usr/bin/ntoaarch64-nm)

# QCC's CMake MOC predefinitions omit CXX_FLAGS and COMPILER_TARGET.
set(CMAKE_C_COMPILER_ARG1 "-Vgcc_ntoaarch64le -D_QNX_SOURCE")
set(CMAKE_CXX_COMPILER_ARG1 "-Vgcc_ntoaarch64le -D_QNX_SOURCE")
set(CMAKE_ASM_FLAGS_INIT "-Vgcc_ntoaarch64le -D_QNX_SOURCE")
