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

# Installed React Native, resolved through the workspace rather than assumed.
set(RNQNX_REACT_NATIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/react-native")
option(RNQNX_FETCH_REACT_NATIVE_DEPS
  "Fetch React Native's pinned C++ third-party dependencies" ON)

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
        vnext/src/fabric/QtQuickMountingLayer.cpp
      )
      target_include_directories(rnqnx_views_qt PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/vnext/include
      )
      target_link_libraries(rnqnx_views_qt PUBLIC Qt6::Quick Qt6::Qml Qt6::Gui)
      rnqnx_enable_strict_warnings(rnqnx_views_qt)

      # Compiling real React Native renderer headers requires folly and glog,
      # which the installed package does not vendor. Fetching is opt-out because
      # it needs network access on a first configure.
      if(RNQNX_FETCH_REACT_NATIVE_DEPS)
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/ReactNativeThirdParty.cmake)

        add_executable(rnqnx_rn_headers_probe
          vnext/probes/rn-headers/main.cpp
        )
        target_link_libraries(rnqnx_rn_headers_probe PRIVATE
          rnqnx_react_native_headers
          rnqnx_views_qt
        )
        rnqnx_enable_strict_warnings(rnqnx_rn_headers_probe)
        set_target_properties(rnqnx_rn_headers_probe PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        add_test(NAME rnqnx_rn_headers_probe
          COMMAND $<TARGET_FILE:rnqnx_rn_headers_probe>)
        set_tests_properties(rnqnx_rn_headers_probe PROPERTIES
          PASS_REGULAR_EXPRESSION
            "RNQNX_RN_HEADERS_OK tag=42 frame=10.0,20.0,100.0x50.0"
        )

        # The adapter needs React Native's out-of-line mutation and shadow-view
        # definitions. These two sources are compiled directly rather than
        # linking a prebuilt React Native, which does not exist for this host.
        add_library(rnqnx_fabric_adapter STATIC
          vnext/src/fabric/ShadowViewMutationAdapter.cpp
          ${RNQNX_REACT_NATIVE_DIR}/ReactCommon/react/renderer/mounting/ShadowViewMutation.cpp
        )
        target_link_libraries(rnqnx_fabric_adapter PUBLIC
          rnqnx_react_native_headers
          rnqnx_views_qt
        )
        rnqnx_enable_strict_warnings(rnqnx_fabric_adapter)

        add_executable(shadow_view_mutation_adapter_host_test
          tests/cpp/ShadowViewMutationAdapterHostTest.cpp
        )
        target_link_libraries(shadow_view_mutation_adapter_host_test
          PRIVATE rnqnx_fabric_adapter)
        rnqnx_enable_strict_warnings(shadow_view_mutation_adapter_host_test)
        set_target_properties(shadow_view_mutation_adapter_host_test PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        add_test(NAME shadow_view_mutation_adapter_host_test
          COMMAND $<TARGET_FILE:shadow_view_mutation_adapter_host_test>
                  -platform offscreen)
      endif()

      add_executable(qtquick_mounting_layer_host_test
        tests/cpp/QtQuickMountingLayerHostTest.cpp
      )
      target_link_libraries(qtquick_mounting_layer_host_test
        PRIVATE rnqnx_views_qt)
      rnqnx_enable_strict_warnings(qtquick_mounting_layer_host_test)

      if(RNQNX_ENABLE_HOST_SANITIZERS AND
         CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
        target_compile_options(qtquick_mounting_layer_host_test PRIVATE
          -fsanitize=address,undefined
          -fno-sanitize-recover=undefined
          -fno-omit-frame-pointer
        )
        target_link_options(qtquick_mounting_layer_host_test PRIVATE
          -fsanitize=address,undefined
        )
      endif()

      set_target_properties(qtquick_mounting_layer_host_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      )
      add_test(NAME qtquick_mounting_layer_host_test
        COMMAND $<TARGET_FILE:qtquick_mounting_layer_host_test>
                -platform offscreen)
      # Qt allocates engine-lifetime singletons that outlive main, so leak
      # detection stays off here exactly as it does for the Screen host test.
      set_tests_properties(qtquick_mounting_layer_host_test PROPERTIES
        ENVIRONMENT
          "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1"
      )

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
