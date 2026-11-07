if(NOT EXISTS "${PROBE_PATH}")
  message(FATAL_ERROR "Missing Pressable probe executable")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env RNQNX_PRESSABLE_INTERCEPT_TEST=1
    QT_QUICK_BACKEND=software ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
    "${PROBE_PATH}" -platform offscreen
  TIMEOUT 20
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result STREQUAL "1" OR NOT
    errors MATCHES "RNQNX_PRESSABLE_FAIL Pressable onPressIn missing")
  message(FATAL_ERROR "Pressable interception failed at wrong boundary: ${result}\n${output}\n${errors}")
endif()
if(output MATCHES "RNQNX_PRESSABLE_OK")
  message(FATAL_ERROR "Interception control falsely claimed Pressable delivery")
endif()
if("${output}\n${errors}" MATCHES
    "AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:")
  message(FATAL_ERROR "Pressable interception emitted sanitizer diagnostics: ${output}\n${errors}")
endif()
message(STATUS "RNQNX_PRESSABLE_INTERCEPTION_REJECTED")
