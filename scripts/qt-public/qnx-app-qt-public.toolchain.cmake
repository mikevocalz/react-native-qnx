# Toolchain for linking an application against the public-source Qt build from
# build-app-qnx.sh. It adds the Qt and QtSvg build trees to the package search
# path (the repository toolchain restricts find_package to the SDP sysroot) and
# makes the application link write DT_RUNPATH, not DT_RPATH (see
# qt612-public-qnx-aarch64le.cmake for why). QT_PUBLIC_BUILD is exported by layout.sh.
if(NOT DEFINED ENV{QT_PUBLIC_BUILD})
  message(FATAL_ERROR "QT_PUBLIC_BUILD is not set; run through build-app-qnx.sh")
endif()
# The source bundle contains this wrapper, not the application checkout.
if(NOT RNQNX_APP_TOOLCHAIN_PATH)
  message(FATAL_ERROR "RNQNX_APP_TOOLCHAIN_PATH must name the checkout's QNX toolchain")
endif()
get_filename_component(_app_toolchain "${RNQNX_APP_TOOLCHAIN_PATH}" ABSOLUTE)
if(NOT EXISTS "${_app_toolchain}")
  message(FATAL_ERROR "Missing application QNX toolchain: ${_app_toolchain}")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES RNQNX_APP_TOOLCHAIN_PATH)
include("${_app_toolchain}")
unset(_app_toolchain)
list(APPEND CMAKE_FIND_ROOT_PATH
  "$ENV{QT_PUBLIC_BUILD}/qnx-build/qtbase"
  "$ENV{QT_PUBLIC_BUILD}/svg-qnx")
foreach(kind SHARED MODULE EXE)
  string(APPEND CMAKE_${kind}_LINKER_FLAGS_INIT " -Wl,--enable-new-dtags")
  if(DEFINED CACHE{CMAKE_${kind}_LINKER_FLAGS})
    set(_flags "$CACHE{CMAKE_${kind}_LINKER_FLAGS}")
    if(NOT _flags MATCHES "(^|[ ;])(-Wl,)?--enable-new-dtags([ ;]|$)")
      set(CMAKE_${kind}_LINKER_FLAGS "${_flags} -Wl,--enable-new-dtags" CACHE STRING "Linker flags" FORCE)
    endif()
    unset(_flags)
  endif()
endforeach()
