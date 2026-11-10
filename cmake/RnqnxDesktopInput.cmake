include_guard(GLOBAL)

option(RNQNX_ENABLE_DESKTOP_INPUT
  "Enable experimental Qt View keyboard, focus and mouse hover delivery" OFF)
if(RNQNX_ENABLE_DESKTOP_INPUT AND NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
  message(FATAL_ERROR "RNQNX_ENABLE_DESKTOP_INPUT requires a Linux, macOS or QNX Qt host")
endif()

function(rnqnx_configure_desktop_input)
  # Mounting-layer layout and consumers must see the same portal/input gate.
  target_compile_definitions(rnqnx_views_qt PUBLIC
    RNQNX_ENABLE_DESKTOP_INPUT=$<BOOL:${RNQNX_ENABLE_DESKTOP_INPUT}>)
  if(RNQNX_ENABLE_DESKTOP_INPUT)
    target_sources(rnqnx_fabric_adapter PRIVATE
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtDesktopInput.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtDesktopInputEvents.cpp")
  endif()
endfunction()

function(rnqnx_add_desktop_input_probe)
  if(NOT RNQNX_ENABLE_DESKTOP_INPUT OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  set(root "${RNQNX_REACT_QT_ROOT}")
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-desktop-input.bundle.js")
  add_custom_command(OUTPUT "${bundle}"
    COMMAND "${root}/node_modules/.bin/metro" build desktop-input-probe.js
      --platform qnx --dev false --max-workers 2 --out "${bundle}"
    WORKING_DIRECTORY "${root}/apps/rn-playground"
    DEPENDS "${root}/apps/rn-playground/desktop-input-probe.js"
      "${root}/apps/rn-playground/metro.config.js"
      "${root}/packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js"
      "${root}/packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js"
      "${root}/pnpm-lock.yaml" VERBATIM)
  add_custom_target(rnqnx_desktop_input_fixture DEPENDS "${bundle}")
  add_executable(rnqnx_qt_controls_consumer "${root}/tests/qt-controls-consumer/main.cpp")
  target_link_libraries(rnqnx_qt_controls_consumer PRIVATE rnqnx_react_host)
  target_compile_definitions(rnqnx_qt_controls_consumer PRIVATE RNQNX_DESKTOP_INPUT_BUNDLE="${bundle}")
  add_dependencies(rnqnx_qt_controls_consumer rnqnx_desktop_input_fixture)
  rnqnx_enable_strict_warnings(rnqnx_qt_controls_consumer)
  set_target_properties(rnqnx_qt_controls_consumer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_ENABLE_HOST_SANITIZERS)
    target_compile_options(rnqnx_qt_controls_consumer PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(rnqnx_qt_controls_consumer PRIVATE -fsanitize=address,undefined)
  endif()
  if(BUILD_TESTING)
    add_test(NAME rnqnx_desktop_input_fabric COMMAND rnqnx_qt_controls_consumer -platform offscreen)
    set_tests_properties(rnqnx_desktop_input_fabric PROPERTIES TIMEOUT 45
      ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endif()
endfunction()
