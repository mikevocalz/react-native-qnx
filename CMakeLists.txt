cmake_minimum_required(VERSION 3.31.9)
project(react-native-qnx VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RNQNX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(RNQNX_ENABLE_HOST_SANITIZERS "Enable ASan and UBSan for host tests" ON)
option(RNQNX_G0_PROBE_ONLY "Build only the G0 C++20 probe on QNX" OFF)

function(rnqnx_enable_strict_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(RNQNX_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(RNQNX_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

include(CTest)

# This target is the executable G0-A language/runtime proof. Compiling it on a
# host validates the TU itself; only a qcc cross-build followed by execution on
# the Pi satisfies G0-A.
if(CMAKE_SYSTEM_NAME STREQUAL "QNX" OR BUILD_TESTING)
  find_package(Threads REQUIRED)
  add_executable(rnqnx_g0_cxx20_probe
    vnext/probes/cxx20/main.cpp
  )
  target_link_libraries(rnqnx_g0_cxx20_probe PRIVATE Threads::Threads)
  rnqnx_enable_strict_warnings(rnqnx_g0_cxx20_probe)
  set_target_properties(rnqnx_g0_cxx20_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )

  if(BUILD_TESTING)
    add_test(NAME rnqnx_g0_cxx20_probe
      COMMAND $<TARGET_FILE:rnqnx_g0_cxx20_probe>)
    set_tests_properties(rnqnx_g0_cxx20_probe PROPERTIES
      PASS_REGULAR_EXPRESSION
        "RNQNX_G0_CXX20_OK ranges=12 designated=21 jthread=34"
    )
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
  message(STATUS "Configuring react-native-qnx for QNX")

  if(NOT RNQNX_G0_PROBE_ONLY)
    # Screen is a window-system probe and possible future bare-Screen path. The
    # component backend is Qt Quick, which remains behind G0-B.
    add_library(qnx_platform STATIC
      native/platform/qnx/screen/QnxScreenSurface.cpp
    )
    target_include_directories(qnx_platform PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/native/platform/qnx
    )
    target_link_libraries(qnx_platform PUBLIC screen)
    rnqnx_enable_strict_warnings(qnx_platform)

    add_executable(rnqnx_playground
      native/platform/qnx/application/main.cpp
    )
    target_link_libraries(rnqnx_playground PRIVATE qnx_platform)
    rnqnx_enable_strict_warnings(rnqnx_playground)
    set_target_properties(rnqnx_playground PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
  endif()

else()
  message(STATUS "Configuring react-native-qnx for host development")

  if(BUILD_TESTING)
    # Compile the concrete QNX Screen wrapper against a test-only emulation of
    # the documented QNX 8 Screen C API. This catches API sequencing, resource
    # ownership, and buffer-management bugs without claiming QNX runtime proof.
    add_executable(qnx_screen_surface_host_test
      native/platform/qnx/screen/QnxScreenSurface.cpp
      tests/cpp/qnx-screen-fake/FakeQnxScreen.cpp
      tests/cpp/QnxScreenSurfaceHostTest.cpp
    )
    target_include_directories(qnx_screen_surface_host_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/native/platform/qnx/screen
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp/qnx-screen-fake
    )
    rnqnx_enable_strict_warnings(qnx_screen_surface_host_test)

    if(RNQNX_ENABLE_HOST_SANITIZERS AND
       CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
      target_compile_options(qnx_screen_surface_host_test PRIVATE
        -fsanitize=address,undefined
        -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer
      )
      target_link_options(qnx_screen_surface_host_test PRIVATE
        -fsanitize=address,undefined
      )
    endif()

    set_target_properties(qnx_screen_surface_host_test PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME qnx_screen_surface_host_test
      COMMAND $<TARGET_FILE:qnx_screen_surface_host_test>)
    set_tests_properties(qnx_screen_surface_host_test PROPERTIES
      ENVIRONMENT
        "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1"
    )

    # Qt Quick view work is developed against an installed host Qt (ADR-0010).
    # It is optional so that a host without Qt still builds and tests the rest.
    find_package(Qt6 6.8 COMPONENTS Core Gui Qml Quick QUIET)
    if(Qt6_FOUND)
      message(STATUS "Qt ${Qt6_VERSION} found; building Qt Quick host targets")

      add_library(rnqnx_views_qt STATIC
        vnext/src/views/qt/QtQuickItemFactory.cpp
      )
      target_include_directories(rnqnx_views_qt PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/vnext/include
      )
      target_link_libraries(rnqnx_views_qt PUBLIC Qt6::Quick Qt6::Qml Qt6::Gui)
      rnqnx_enable_strict_warnings(rnqnx_views_qt)

      add_executable(rnqnx_qtquick_host_probe
        vnext/probes/qtquick/main.cpp
      )
      target_link_libraries(rnqnx_qtquick_host_probe PRIVATE rnqnx_views_qt)
      rnqnx_enable_strict_warnings(rnqnx_qtquick_host_probe)
      set_target_properties(rnqnx_qtquick_host_probe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      )

      # Offscreen keeps the test headless. A windowed run is a manual command,
      # and neither one is evidence for the Pi.
      add_test(NAME rnqnx_qtquick_host_probe
        COMMAND $<TARGET_FILE:rnqnx_qtquick_host_probe> -platform offscreen)
      set_tests_properties(rnqnx_qtquick_host_probe PROPERTIES
        PASS_REGULAR_EXPRESSION "RNQNX_QTQUICK_HOST_OK items=1000"
      )
    else()
      message(STATUS "Qt6 >= 6.8 not found; skipping Qt Quick host targets")
    endif()
  endif()

endif()
