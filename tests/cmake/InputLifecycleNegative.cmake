if(NOT EXISTS "${PROBE_PATH}")
  message(FATAL_ERROR "Missing input probe executable")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env RNQNX_INPUT_INTERCEPT_TEST=1
    "${PROBE_PATH}" -platform offscreen
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result STREQUAL "1" OR NOT
    errors MATCHES "RNQNX_INPUT_LIFECYCLE_FAIL JavaScript input event count mismatch")
  message(FATAL_ERROR "Interception control failed at the wrong boundary: ${result}\n${output}\n${errors}")
endif()
if(output MATCHES "RNQNX_INPUT_JS_OK")
  message(FATAL_ERROR "Interception control falsely claimed JS delivery")
endif()
if("${output}\n${errors}" MATCHES
    "AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:")
  message(FATAL_ERROR "Interception control emitted sanitizer diagnostics: ${output}\n${errors}")
endif()
message(STATUS "RNQNX_INPUT_JS_INTERCEPTION_REJECTED expected missing JS delivery")
