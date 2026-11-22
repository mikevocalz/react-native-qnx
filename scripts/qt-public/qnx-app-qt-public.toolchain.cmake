# Toolchain for linking an application against the public-source Qt build from
# build-app-qnx.sh. It adds the Qt and QtSvg build trees to the package search
# path (the repository toolchain restricts find_package to the SDP sysroot) and
# makes the application link write DT_RUNPATH, not DT_RPATH (see
# qt612-public-qnx-aarch64le.cmake for why). QT_PUBLIC_BUILD is exported by layout.sh.
if(NOT DEFINED ENV{QT_PUBLIC_BUILD})
  message(FATAL_ERROR "QT_PUBLIC_BUILD is not set; run through build-app-qnx.sh")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../vnext/cmake/qnx-aarch64le.toolchain.cmake")
list(APPEND CMAKE_FIND_ROOT_PATH
  "$ENV{QT_PUBLIC_BUILD}/qnx-build/qtbase"
  "$ENV{QT_PUBLIC_BUILD}/svg-qnx")
foreach(kind SHARED MODULE EXE)
  string(APPEND CMAKE_${kind}_LINKER_FLAGS_INIT " -Wl,--enable-new-dtags")
endforeach()
