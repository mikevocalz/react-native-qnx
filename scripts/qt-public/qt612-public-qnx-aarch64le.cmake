# Same as the evaluation's qt612-qnx-aarch64le.cmake, pointed at the public source tree.
# QT_PUBLIC_SRC is exported by layout.sh (configure-qnx.sh).
if(NOT DEFINED ENV{QT_PUBLIC_SRC})
  message(FATAL_ERROR "QT_PUBLIC_SRC is not set; run through configure-qnx.sh")
endif()
include($ENV{QT_PUBLIC_SRC}/coin/provisioning/common/shared/cmake_toolchain_files/qnx-toolchain-aarch64le.cmake)
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

# QNX's ld writes -rpath as DT_RPATH, which the loader searches before
# LD_LIBRARY_PATH. DT_RUNPATH is searched after it, so a replacement library on
# LD_LIBRARY_PATH wins. Qt's enable_new_dtags feature cannot be turned on here
# (QT_FEATURE_elf is OFF on QNX), so pass the linker flag directly.
foreach(kind SHARED MODULE EXE)
  if(NOT CMAKE_${kind}_LINKER_FLAGS_INIT MATCHES "(^|[ ;])-Wl,--enable-new-dtags[ ;]*$")
    string(APPEND CMAKE_${kind}_LINKER_FLAGS_INIT " -Wl,--enable-new-dtags")
  endif()
  # _INIT does not refresh an existing cache. Keep effective user flags and
  # make the canonical enable flag last, including after an earlier disable.
  if(DEFINED CACHE{CMAKE_${kind}_LINKER_FLAGS})
    set(_flags "${CMAKE_${kind}_LINKER_FLAGS}")
    if(NOT _flags MATCHES "(^|[ ;])-Wl,--enable-new-dtags[ ;]*$")
      string(APPEND _flags " -Wl,--enable-new-dtags")
    endif()
    set(CMAKE_${kind}_LINKER_FLAGS "${_flags}" CACHE STRING "Linker flags" FORCE)
    set(CMAKE_${kind}_LINKER_FLAGS "${_flags}")
    unset(_flags)
  endif()
endforeach()
