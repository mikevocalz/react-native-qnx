include_guard(GLOBAL)
function(rnqnx_add_text_input_probe)
  if(TARGET rnqnx_text_input_probe)
    return()
  endif()
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-text-input-probe.bundle.js")
  set(RNQNX_TEXT_INPUT_TEST_BUNDLE "" CACHE FILEPATH "Host-produced production TextInput Metro bundle for QNX")
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX" AND RNQNX_TEXT_INPUT_TEST_BUNDLE)
    if(NOT EXISTS "${RNQNX_TEXT_INPUT_TEST_BUNDLE}" OR IS_DIRECTORY "${RNQNX_TEXT_INPUT_TEST_BUNDLE}")
      message(FATAL_ERROR "RNQNX_TEXT_INPUT_TEST_BUNDLE must name an existing file")
    endif()
    set(bundle "${RNQNX_TEXT_INPUT_TEST_BUNDLE}")
  else()
    add_custom_command(OUTPUT "${bundle}"
      COMMAND "${CMAKE_SOURCE_DIR}/node_modules/.bin/metro" build text-input-probe.js
        --platform qnx --dev false --out "${bundle}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/apps/rn-playground"
      DEPENDS "${root}/apps/rn-playground/text-input-probe.js"
        "${root}/apps/rn-playground/metro.config.js" "${root}/apps/rn-playground/babel.config.js"
        "${root}/packages/react-native-qnx/Libraries/Components/TextInput/TextInput.js"
        "${root}/pnpm-lock.yaml" VERBATIM)
  endif()
  set(fixture "${bundle}")
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(RNQNX_INPUT_TEST_HERMESC "" CACHE FILEPATH "Pinned 260318099.0.5 host hermesc for native input probes")
    if(NOT EXISTS "${RNQNX_INPUT_TEST_HERMESC}")
      message(FATAL_ERROR "Supply RNQNX_INPUT_TEST_HERMESC from pinned hermes-compiler 260318099.0.5")
    endif()
    set(fixture "${CMAKE_BINARY_DIR}/text-input-probe.hbc")
    add_custom_command(OUTPUT "${fixture}"
      COMMAND "${RNQNX_INPUT_TEST_HERMESC}" -O -emit-binary -out "${fixture}" "${bundle}"
      DEPENDS "${bundle}" "${RNQNX_INPUT_TEST_HERMESC}" VERBATIM)
  endif()
  add_custom_target(rnqnx_text_input_fixture DEPENDS "${fixture}")
  add_executable(rnqnx_text_input_probe "${root}/tests/cpp/QtTextInputProbe.cpp")
  target_link_libraries(rnqnx_text_input_probe PRIVATE rnqnx_react_host)
  target_compile_definitions(rnqnx_text_input_probe PRIVATE RNQNX_TEXT_INPUT_FIXTURE="${fixture}")
  add_dependencies(rnqnx_text_input_probe rnqnx_text_input_fixture)
  add_executable(rnqnx_text_input_bridge_test "${root}/tests/native/text-input/BridgeTest.cpp")
  target_link_libraries(rnqnx_text_input_bridge_test PRIVATE rnqnx_views_qt)
  foreach(target rnqnx_text_input_probe rnqnx_text_input_bridge_test)
    rnqnx_enable_strict_warnings(${target})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX" AND RNQNX_ENABLE_HOST_SANITIZERS)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endforeach()
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX" AND BUILD_TESTING)
    add_test(NAME rnqnx_text_input_probe COMMAND rnqnx_text_input_probe -platform offscreen)
    add_test(NAME rnqnx_text_input_bridge_test COMMAND rnqnx_text_input_bridge_test -platform offscreen)
    add_test(NAME rnqnx_text_input_rejects_interception COMMAND "${CMAKE_COMMAND}"
      "-DPROBE=$<TARGET_FILE:rnqnx_text_input_probe>" -P "${root}/tests/cmake/TextInputNegative.cmake")
    set_tests_properties(rnqnx_text_input_probe rnqnx_text_input_bridge_test rnqnx_text_input_rejects_interception
      PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endif()
endfunction()
