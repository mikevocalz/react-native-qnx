execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env RNQNX_CONSUMER_INPUT_INTERCEPT=1
    "${PROBE}" -platform offscreen
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 15)
if(NOT result STREQUAL "1" OR NOT errors MATCHES "RNQNX_CONSUMER_INPUT_FAIL waiting for first press" OR
   "${output}\n${errors}" MATCHES "AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:|RNQNX_CONSUMER_INPUT_OK")
  message(FATAL_ERROR "Window interception did not fail at the first React press: ${result}\n${output}\n${errors}")
endif()
message(STATUS "RNQNX_CONSUMER_WINDOW_INTERCEPTION_REJECTED")
