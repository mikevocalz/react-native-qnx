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
endfunction()
