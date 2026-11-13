# Stage 5 WebGPU test app. Built against a Dawn build tree produced by
# scripts/webgpu/build-dawn.sh (cross-compiled for QNX) or by a host Dawn build
# (null backend, used for CI). Not part of the default repository build.
cmake_minimum_required(VERSION 3.25)
project(rnqnx_webgpu_test LANGUAGES CXX)

if(CMAKE_SYSTEM_NAME STREQUAL "QNX" AND DEFINED ENV{QNX_TARGET})
  # CMake detected the build host's /usr/include as the compiler's implicit include
  # directory, so it does not drop the SDP's sysroot include directory that Qt's
  # imported targets add. That directory then lands ahead of libc++ and breaks
  # its #include_next chain. Tell CMake the sysroot directories are implicit.
  list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES
    "$ENV{QNX_TARGET}/usr/include" "$ENV{QNX_TARGET}/usr/include/c++/v1")
endif()

set(RNQNX_DAWN_SOURCE_DIR "" CACHE PATH "Dawn source tree (pinned checkout)")
set(RNQNX_DAWN_BUILD_DIR "" CACHE PATH "Dawn build tree containing libwebgpu_dawn.a and gen/")
if(NOT RNQNX_DAWN_SOURCE_DIR OR NOT RNQNX_DAWN_BUILD_DIR)
  message(FATAL_ERROR "Set RNQNX_DAWN_SOURCE_DIR and RNQNX_DAWN_BUILD_DIR.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The monolithic static library lands under the build tree (for example
# src/dawn/native/libwebgpu_dawn.a); search for it instead of hard-coding.
file(GLOB_RECURSE _rnqnx_dawn_libs "${RNQNX_DAWN_BUILD_DIR}/libwebgpu_dawn.a")
if(NOT _rnqnx_dawn_libs)
  message(FATAL_ERROR "libwebgpu_dawn.a not found under ${RNQNX_DAWN_BUILD_DIR}")
endif()
list(GET _rnqnx_dawn_libs 0 RNQNX_WEBGPU_DAWN_LIB)
message(STATUS "Dawn library: ${RNQNX_WEBGPU_DAWN_LIB}")

add_executable(rnqnx_webgpu_test
  testapp/Check.cpp
  testapp/GpuContext.cpp
  testapp/ImageWriter.cpp
  testapp/Scenarios.cpp
  testapp/ScreenWindow.cpp
  testapp/SurfacePresenter.cpp
  testapp/main.cpp)
# Definitions for libc++ members that clang-compiled Dawn objects need and the
# SDP's libc++.so lacks; see shim/LibcxxShim.cpp.
option(RNQNX_WEBGPU_LIBCXX_SHIM "Link the libc++ member shim (needed for a clang-built Dawn)" OFF)
if(RNQNX_WEBGPU_LIBCXX_SHIM)
  target_sources(rnqnx_webgpu_test PRIVATE shim/LibcxxShim.cpp)
endif()
target_include_directories(rnqnx_webgpu_test PRIVATE
  "${RNQNX_DAWN_SOURCE_DIR}/include"
  "${RNQNX_DAWN_BUILD_DIR}/gen/include")
target_link_libraries(rnqnx_webgpu_test PRIVATE "${RNQNX_WEBGPU_DAWN_LIB}")
find_package(Threads REQUIRED)
target_link_libraries(rnqnx_webgpu_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
  # EGL is also loaded by Dawn at runtime; linking it lets ScreenWindow query configs.
  target_link_libraries(rnqnx_webgpu_test PRIVATE screen EGL GLESv2)
endif()

# Qt Quick composition test (stage 5): WebGPU frames shown in a Qt Quick scene
# graph, with QtGraphicsSurface observing the window. Needs a Qt 6 with Quick.
option(RNQNX_WEBGPU_QT_COMPOSE "Build the Qt Quick composition test" OFF)
if(RNQNX_WEBGPU_QT_COMPOSE)
  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Quick)
  set(RNQNX_ENABLE_QT_GRAPHICS ON)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../qt/graphics" qt-graphics)
  set(CMAKE_AUTOMOC ON)
  add_executable(rnqnx_webgpu_qt_compose
    testapp/GpuContext.cpp testapp/Scenarios.cpp
    qt-compose/WebGpuFrameSource.cpp qt-compose/WebGpuItem.cpp qt-compose/main.cpp)
  if(RNQNX_WEBGPU_LIBCXX_SHIM)
    target_sources(rnqnx_webgpu_qt_compose PRIVATE shim/LibcxxShim.cpp)
  endif()
  target_include_directories(rnqnx_webgpu_qt_compose PRIVATE
    testapp "${RNQNX_DAWN_SOURCE_DIR}/include" "${RNQNX_DAWN_BUILD_DIR}/gen/include")
  target_link_libraries(rnqnx_webgpu_qt_compose PRIVATE
    rnqnx_qt_graphics Qt6::Core Qt6::Gui Qt6::Quick "${RNQNX_WEBGPU_DAWN_LIB}" Threads::Threads)
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    target_link_libraries(rnqnx_webgpu_qt_compose PRIVATE screen EGL GLESv2)
  endif()
endif()
