include_guard(GLOBAL)
function(rnqnx_add_accessibility_action_probe)
  if(TARGET rnqnx_accessibility_action_probe)
    return()
  endif()
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-accessibility-action-probe.bundle.js")
  set(RNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE "" CACHE FILEPATH "Host-produced production accessibility action Metro bundle for QNX")
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX" AND RNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE)
    if(NOT EXISTS "${RNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE}" OR IS_DIRECTORY "${RNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE}")
      message(FATAL_ERROR "RNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE must name an existing file")
    endif()
    set(bundle "${RNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE}")
  else()
    add_custom_command(OUTPUT "${bundle}"
      COMMAND "${CMAKE_SOURCE_DIR}/node_modules/.bin/metro" build accessibility-action-probe.js
        --platform qnx --dev false --out "${bundle}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/apps/rn-playground"
      DEPENDS "${root}/apps/rn-playground/accessibility-action-probe.js"
        "${root}/apps/rn-playground/metro.config.js" "${root}/apps/rn-playground/babel.config.js"
        "${root}/packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js"
        "${root}/pnpm-lock.yaml" VERBATIM)
  endif()
  set(fixture "${bundle}")
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(RNQNX_ACCESSIBILITY_ACTION_TEST_HERMESC "" CACHE FILEPATH "Pinned hermes-compiler 260318099.0.5 host hermesc")
    if(NOT EXISTS "${RNQNX_ACCESSIBILITY_ACTION_TEST_HERMESC}" OR IS_DIRECTORY "${RNQNX_ACCESSIBILITY_ACTION_TEST_HERMESC}")
      message(FATAL_ERROR "Supply RNQNX_ACCESSIBILITY_ACTION_TEST_HERMESC from pinned hermes-compiler 260318099.0.5")
    endif()
    set(fixture "${CMAKE_BINARY_DIR}/accessibility-action-probe.hbc")
    add_custom_command(OUTPUT "${fixture}"
      COMMAND "${RNQNX_ACCESSIBILITY_ACTION_TEST_HERMESC}" -O -emit-binary -out "${fixture}" "${bundle}"
      DEPENDS "${bundle}" "${RNQNX_ACCESSIBILITY_ACTION_TEST_HERMESC}" VERBATIM)
  endif()
  add_custom_target(rnqnx_accessibility_action_fixture DEPENDS "${fixture}")
  add_executable(rnqnx_accessibility_action_probe "${root}/tests/cpp/QtAccessibilityActionProbe.cpp")
  target_link_libraries(rnqnx_accessibility_action_probe PRIVATE rnqnx_react_host)
  target_compile_definitions(rnqnx_accessibility_action_probe PRIVATE RNQNX_ACCESSIBILITY_ACTION_FIXTURE="${fixture}")
  add_dependencies(rnqnx_accessibility_action_probe rnqnx_accessibility_action_fixture)
  add_executable(rnqnx_accessibility_action_native_test "${root}/tests/native/accessibility/ActionTest.cpp")
  target_link_libraries(rnqnx_accessibility_action_native_test PRIVATE rnqnx_views_qt)
  foreach(target rnqnx_accessibility_action_probe rnqnx_accessibility_action_native_test)
    rnqnx_enable_strict_warnings(${target})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX" AND RNQNX_ENABLE_HOST_SANITIZERS)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endforeach()
  if(BUILD_TESTING AND NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
    add_test(NAME rnqnx_accessibility_action_probe COMMAND rnqnx_accessibility_action_probe -platform offscreen)
    add_test(NAME rnqnx_accessibility_action_native_test COMMAND rnqnx_accessibility_action_native_test -platform offscreen)
    add_test(NAME rnqnx_accessibility_action_rejects_interception COMMAND "${CMAKE_COMMAND}"
      "-DPROBE=$<TARGET_FILE:rnqnx_accessibility_action_probe>" -P "${root}/tests/cmake/AccessibilityActionNegative.cmake")
    add_test(NAME rnqnx_accessibility_action_rejects_registration COMMAND "${CMAKE_COMMAND}"
      "-DPROBE=$<TARGET_FILE:rnqnx_accessibility_action_probe>" -P "${root}/tests/cmake/AccessibilityActionRegistration.cmake")
    set_tests_properties(rnqnx_accessibility_action_probe rnqnx_accessibility_action_native_test
      rnqnx_accessibility_action_rejects_interception rnqnx_accessibility_action_rejects_registration
      PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endif()
endfunction()
