include_guard(GLOBAL)

function(rnqnx_add_input_lifecycle_probe)
  if(NOT RNQNX_ENABLE_INPUT_LIFECYCLE)
    return()
  endif()
  add_executable(rnqnx_input_lifecycle_probe
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/cpp/QtInputLifecycleProbe.cpp")
  target_link_libraries(rnqnx_input_lifecycle_probe PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_input_lifecycle_probe)
  set_target_properties(rnqnx_input_lifecycle_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  set(fixture "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/fixtures/input-lifecycle/setup.js")
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(RNQNX_INPUT_TEST_HERMESC "" CACHE FILEPATH "Pinned 260318099.0.5 host hermesc for input probe")
    if(NOT EXISTS "${RNQNX_INPUT_TEST_HERMESC}")
      message(FATAL_ERROR "Supply RNQNX_INPUT_TEST_HERMESC from pinned hermes-compiler 260318099.0.5")
    endif()
    set(compiled_fixture "${CMAKE_BINARY_DIR}/input-lifecycle-setup.hbc")
    add_custom_command(OUTPUT "${compiled_fixture}"
      COMMAND "${RNQNX_INPUT_TEST_HERMESC}" -O -emit-binary -out "${compiled_fixture}" "${fixture}"
      DEPENDS "${fixture}" "${RNQNX_INPUT_TEST_HERMESC}" VERBATIM)
    add_custom_target(rnqnx_input_lifecycle_fixture DEPENDS "${compiled_fixture}")
    add_dependencies(rnqnx_input_lifecycle_probe rnqnx_input_lifecycle_fixture)
    set(fixture "${compiled_fixture}")
  endif()
  target_compile_definitions(rnqnx_input_lifecycle_probe PRIVATE RNQNX_INPUT_FIXTURE="${fixture}")
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX" AND BUILD_TESTING)
    add_test(NAME rnqnx_input_lifecycle_probe COMMAND rnqnx_input_lifecycle_probe -platform offscreen)
    add_test(NAME rnqnx_input_lifecycle_rejects_interception COMMAND "${CMAKE_COMMAND}"
      "-DPROBE_PATH=$<TARGET_FILE:rnqnx_input_lifecycle_probe>"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/cmake/InputLifecycleNegative.cmake")
    set_tests_properties(rnqnx_input_lifecycle_probe rnqnx_input_lifecycle_rejects_interception PROPERTIES TIMEOUT 30
      ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endif()

  # Keep the raw payload fixture above; this separate production Metro fixture
  # exercises React's responder negotiation and actual Pressability callbacks.
  set(pressable_bundle "${CMAKE_BINARY_DIR}/rnqnx-pressable-probe.bundle.js")
  add_custom_command(OUTPUT "${pressable_bundle}"
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/.bin/metro" build
      pressable-probe.js --platform qnx --dev false --out "${pressable_bundle}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/pressable-probe.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/metro.config.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/babel.config.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/metro-config-qnx/src/index.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/pnpm-lock.yaml"
    VERBATIM)
  set(pressable_fixture "${pressable_bundle}")
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(pressable_fixture "${CMAKE_BINARY_DIR}/pressable-probe.hbc")
    add_custom_command(OUTPUT "${pressable_fixture}"
      COMMAND "${RNQNX_INPUT_TEST_HERMESC}" -O -emit-binary
        -out "${pressable_fixture}" "${pressable_bundle}"
      DEPENDS "${pressable_bundle}" "${RNQNX_INPUT_TEST_HERMESC}" VERBATIM)
  endif()
  add_custom_target(rnqnx_pressable_fixture DEPENDS "${pressable_fixture}")
  add_executable(rnqnx_pressable_probe
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/cpp/QtPressableProbe.cpp")
  target_link_libraries(rnqnx_pressable_probe PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_pressable_probe)
  add_dependencies(rnqnx_pressable_probe rnqnx_pressable_fixture)
  target_compile_definitions(rnqnx_pressable_probe PRIVATE RNQNX_PRESSABLE_FIXTURE="${pressable_fixture}")
  set_target_properties(rnqnx_pressable_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX" AND BUILD_TESTING)
    add_test(NAME rnqnx_pressable_probe COMMAND rnqnx_pressable_probe -platform offscreen)
    add_test(NAME rnqnx_pressable_rejects_interception COMMAND "${CMAKE_COMMAND}"
      "-DPROBE_PATH=$<TARGET_FILE:rnqnx_pressable_probe>"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/cmake/PressableNegative.cmake")
    add_test(NAME rnqnx_pressable_rejects_disabled_cancellation COMMAND "${CMAKE_COMMAND}"
      "-DPROBE_PATH=$<TARGET_FILE:rnqnx_pressable_probe>"
      -DCONTROL=removed-target
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/cmake/PressableNegative.cmake")
    set_tests_properties(rnqnx_pressable_probe rnqnx_pressable_rejects_interception
      rnqnx_pressable_rejects_disabled_cancellation PROPERTIES TIMEOUT 30
      ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endif()
endfunction()
