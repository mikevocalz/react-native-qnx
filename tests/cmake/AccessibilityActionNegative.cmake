if(NOT DEFINED PROBE)
  message(FATAL_ERROR "PROBE required")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env RNQNX_ACCESSIBILITY_ACTION_INTERCEPT_TEST=1 "${PROBE}" -platform offscreen
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 20)
set(log "${output}\n${errors}")
if(NOT "${result}" STREQUAL "1" OR NOT log MATCHES "RNQNX_ACCESSIBILITY_ACTION_FAIL JavaScript accessibility action count mismatch" OR
    log MATCHES "RNQNX_ACCESSIBILITY_ACTION_OK|AddressSanitizer|runtime error:|UndefinedBehaviorSanitizer")
  message(FATAL_ERROR "Interception must fail at the JS action boundary with exit 1: result=${result}\n${log}")
endif()
message(STATUS "Accessibility action interception rejected at the JavaScript boundary")
