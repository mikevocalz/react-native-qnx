if(NOT DEFINED PROBE)
  message(FATAL_ERROR "PROBE required")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env RNQNX_ACCESSIBILITY_ACTION_UNSUPPORTED_TEST=1 "${PROBE}" -platform offscreen
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 20)
set(log "${output}\n${errors}")
if(NOT "${result}" STREQUAL "0" OR NOT log MATCHES "RNQNX_ACCESSIBILITY_ACTION_REJECT_OK unsupported-registration" OR
    log MATCHES "RNQNX_ACCESSIBILITY_ACTION_OK|RNQNX_ACCESSIBILITY_ACTION_FAIL|AddressSanitizer|runtime error:|UndefinedBehaviorSanitizer")
  message(FATAL_ERROR "Unsupported registration must reject with exit 0 and its exact marker: result=${result}\n${log}")
endif()
message(STATUS "Unsupported accessibility action registration rejected explicitly")
