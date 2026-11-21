include(/home/qnxbuild/devx3/src/vnext/cmake/qnx-aarch64le.toolchain.cmake)
# The public Qt build lives outside the SDP sysroot; let package lookup reach it.
list(APPEND CMAKE_FIND_ROOT_PATH /home/qnxbuild/qt-6.12.0-public/qnx-build/qtbase)
list(APPEND CMAKE_FIND_ROOT_PATH /home/qnxbuild/qtws-qnx)
list(APPEND CMAKE_FIND_ROOT_PATH /home/qnxbuild/qt-6.12.0-public/svg-qnx)
