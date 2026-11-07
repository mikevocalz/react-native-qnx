execute_process(COMMAND "${CMAKE_COMMAND}" -E env RNQNX_TEXT_INPUT_INTERCEPT_TEST=1
  "${PROBE}" -platform offscreen RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 25)
set(combined "${output}${error}")
if(NOT "${status}" STREQUAL "1" OR NOT combined MATCHES "JavaScript TextInput change count mismatch" OR
   combined MATCHES "RNQNX_TEXT_INPUT_OK|AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:")
  message(FATAL_ERROR "TextInput interception control failed: ${status}\n${combined}")
endif()
