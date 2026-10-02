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

# Hermes is a long build and is not needed by most changes, so it is opt-in:
#   cmake --preset macos-host-tests -DRNQNX_BUILD_HERMES=ON
option(RNQNX_BUILD_HERMES "Build Hermes from source for the host" OFF)
option(RNQNX_BUILD_HERMES_QNX "Link the cross-built QNX Hermes/JSI probe" OFF)
option(RNQNX_BUILD_NITRO_QNX "Build the QNX Nitro shared C++ runtime" OFF)
option(RNQNX_BUILD_EXPO_MODULES_QNX "Build the QNX Expo Modules JSI bootstrap" OFF)
option(RNQNX_BUILD_REACT_QNX
  "Build the full React Native + Hermes + Fabric + Qt Quick QNX target graph" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RnqnxReactQt.cmake)

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
        "RNQNX_G0_CXX20_OK ranges=12 designated=21 (jthread=34|thread=34 jthread=unsupported)"
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

  if(RNQNX_BUILD_NITRO_QNX)
    add_subdirectory(packages/qnx-nitro-runtime)
  endif()

  if(RNQNX_BUILD_EXPO_MODULES_QNX)
    add_subdirectory(packages/expo-modules-core-qnx)
  endif()

  if(RNQNX_BUILD_HERMES_QNX)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/HermesQNX.cmake)

    add_executable(rnqnx_hermes_qnx_probe
      vnext/probes/hermes/main.cpp
    )
    target_link_libraries(rnqnx_hermes_qnx_probe PRIVATE
      rnqnx_hermes
      Threads::Threads
    )
    rnqnx_enable_strict_warnings(rnqnx_hermes_qnx_probe)
    set_target_properties(rnqnx_hermes_qnx_probe PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    if(RNQNX_BUILD_NITRO_QNX AND RNQNX_BUILD_EXPO_MODULES_QNX)
      add_executable(rnqnx_runtime_qnx_probe
        vnext/probes/runtime-qnx/main.cpp
      )
      target_link_libraries(rnqnx_runtime_qnx_probe PRIVATE
        rnqnx_nitro_qnx
        rnqnx_expo_modules_qnx
        rnqnx_hermes
        Threads::Threads
      )
      rnqnx_enable_strict_warnings(rnqnx_runtime_qnx_probe)
      set_target_properties(rnqnx_runtime_qnx_probe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      )
    endif()
  endif()

  if(RNQNX_BUILD_REACT_QNX)
    if(NOT RNQNX_BUILD_HERMES_QNX)
      message(FATAL_ERROR
        "RNQNX_BUILD_REACT_QNX requires RNQNX_BUILD_HERMES_QNX=ON")
    endif()
    if(NOT RNQNX_FETCH_REACT_NATIVE_DEPS)
      message(FATAL_ERROR
        "RNQNX_BUILD_REACT_QNX requires RNQNX_FETCH_REACT_NATIVE_DEPS=ON")
    endif()

    find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick)
    message(STATUS
      "Qt ${Qt6_VERSION} found; building full React/Fabric/Qt Quick QNX targets")

    rnqnx_add_qt_views_target()
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/ReactNativeThirdParty.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/ReactNativeRenderer.cmake)
    rnqnx_add_fabric_adapter_target()
    rnqnx_add_react_host_target()

    add_executable(rnqnx_react_qnx_probe
      vnext/probes/react-qnx/main.cpp
    )
    target_link_libraries(rnqnx_react_qnx_probe PRIVATE
      rnqnx_react_host
      Qt6::Quick
      Qt6::Qml
      Qt6::Gui
    )
    rnqnx_enable_strict_warnings(rnqnx_react_qnx_probe)
    set_target_properties(rnqnx_react_qnx_probe PROPERTIES
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
    # The host component list is the source of truth; see docs/build-qt-modules.md.
    set(RNQNX_QT_HOST_MODULES Core Gui Qml Quick)
    find_package(Qt6 6.11.1 COMPONENTS ${RNQNX_QT_HOST_MODULES} QUIET)
    if(Qt6_FOUND)
      message(STATUS "Qt ${Qt6_VERSION} found; building Qt Quick host targets (minimum 6.11.1)")

      rnqnx_add_qt_views_target()

      # Compiling real React Native renderer headers requires folly and glog,
      # which the installed package does not vendor. Fetching is opt-out because
      # it needs network access on a first configure.
      if(RNQNX_FETCH_REACT_NATIVE_DEPS)
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/ReactNativeThirdParty.cmake)
        if(RNQNX_BUILD_HERMES)
          include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/Hermes.cmake)

          add_executable(rnqnx_hermes_probe vnext/probes/hermes/main.cpp)
          target_link_libraries(rnqnx_hermes_probe PRIVATE rnqnx_hermes)
          rnqnx_enable_strict_warnings(rnqnx_hermes_probe)
          set_target_properties(rnqnx_hermes_probe PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )
          add_test(NAME rnqnx_hermes_probe
            COMMAND $<TARGET_FILE:rnqnx_hermes_probe>)
          set_tests_properties(rnqnx_hermes_probe PROPERTIES
            PASS_REGULAR_EXPRESSION
              "RNQNX_HERMES_OK arithmetic es6 async-arrow host-function"
          )

          # The runtime/renderer host: the five pieces React Native ships no
          # generic implementation of. Built after the renderer modules, so the
          # target is declared here and its sources added below.
          set(RNQNX_BUILD_REACT_HOST TRUE)
        endif()

        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/ReactNativeRenderer.cmake)

        if(RNQNX_BUILD_REACT_HOST)
          rnqnx_add_react_host_target()

          add_executable(qt_react_host_host_test
            tests/cpp/QtReactHostHostTest.cpp
          )
          target_link_libraries(qt_react_host_host_test PRIVATE rnqnx_react_host)
          rnqnx_enable_strict_warnings(qt_react_host_host_test)
          set_target_properties(qt_react_host_host_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )
          # Bundle the playground with Metro so the test can load real React
          # output rather than hand-written JavaScript. The bundle is a build
          # artifact, not a checked-in file.
          set(RNQNX_PLAYGROUND_BUNDLE "${CMAKE_BINARY_DIR}/rnqnx-playground.bundle.js")
          add_custom_command(
            OUTPUT "${RNQNX_PLAYGROUND_BUNDLE}"
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/.bin/metro" build
                    index.js --platform qnx --dev false
                    --out "${RNQNX_PLAYGROUND_BUNDLE}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground"
            DEPENDS
              "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/index.js"
              "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/metro.config.js"
              "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/qnx-asset.png"
              "${CMAKE_CURRENT_SOURCE_DIR}/packages/metro-config-qnx/src/index.js"
              "${CMAKE_CURRENT_SOURCE_DIR}/packages/react-native-qnx/Libraries/Utilities/Platform.js"
              "${CMAKE_CURRENT_SOURCE_DIR}/packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js"
            COMMENT "Bundling apps/rn-playground for qnx"
            VERBATIM
          )
          add_custom_target(rnqnx_playground_bundle
            DEPENDS "${RNQNX_PLAYGROUND_BUNDLE}")
          add_dependencies(qt_react_host_host_test rnqnx_playground_bundle)
          target_compile_definitions(qt_react_host_host_test PRIVATE
            RNQNX_PLAYGROUND_BUNDLE_PATH="${RNQNX_PLAYGROUND_BUNDLE}")

          # A runnable window, so the rendered app can be looked at rather than
          # only asserted. Also renders to a PNG for machines with no display.
          add_executable(rnqnx_playground_host vnext/probes/playground/main.cpp)
          target_link_libraries(rnqnx_playground_host PRIVATE rnqnx_react_host)
          rnqnx_enable_strict_warnings(rnqnx_playground_host)
          add_dependencies(rnqnx_playground_host rnqnx_playground_bundle)
          target_compile_definitions(rnqnx_playground_host PRIVATE
            RNQNX_PLAYGROUND_BUNDLE_PATH="${RNQNX_PLAYGROUND_BUNDLE}")
          set_target_properties(rnqnx_playground_host PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
          )

          # ASan's allocator is installed process-wide by interposition, so
          # heap use-after-free is caught in the React Native and Qt libraries
          # this links even though they are not themselves instrumented. That
          # is what makes the event-beat lifetime case a real test rather than
          # a coin flip: without it, reading a destroyed beat only sometimes
          # touches an unmapped page.
          if(RNQNX_ENABLE_HOST_SANITIZERS AND
             CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
            target_compile_options(qt_react_host_host_test PRIVATE
              -fsanitize=address
              -fno-omit-frame-pointer
            )
            target_link_options(qt_react_host_host_test PRIVATE
              -fsanitize=address
            )
          endif()

          add_test(NAME qt_react_host_host_test
            COMMAND $<TARGET_FILE:qt_react_host_host_test> -platform offscreen)
          # Qt and Hermes both allocate singletons that outlive main, exactly as
          # for the other sanitized host tests.
          set_tests_properties(qt_react_host_host_test PROPERTIES
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
          )
        endif()

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

        rnqnx_add_fabric_adapter_target()

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


        # The full pipeline: a real ShadowTree commit, React Native's differ,
        # MountingCoordinator, the adapter, and Yoga's geometry on Qt items.
        add_executable(shadow_tree_mounting_host_test
          tests/cpp/ShadowTreeMountingHostTest.cpp
        )
        target_link_libraries(shadow_tree_mounting_host_test
          PRIVATE rnqnx_fabric_adapter)
        rnqnx_enable_strict_warnings(shadow_tree_mounting_host_test)
        set_target_properties(shadow_tree_mounting_host_test PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        add_test(NAME shadow_tree_mounting_host_test
          COMMAND $<TARGET_FILE:shadow_tree_mounting_host_test>
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
      message(STATUS "Qt6 >= 6.11.1 not found; skipping Qt Quick host targets")
    endif()
  endif()

endif()
