if(NOT EXISTS "${PROBE_PATH}")
  message(FATAL_ERROR "Missing Pressable probe executable")
endif()
if(NOT CONTROL OR CONTROL STREQUAL "interception")
  set(control_env "RNQNX_PRESSABLE_INTERCEPT_TEST=1")
  set(expected_failure "Pressable onPressIn missing")
  set(control_marker "RNQNX_PRESSABLE_INTERCEPTION_REJECTED")
elseif(CONTROL STREQUAL "removed-target")
  set(control_env "RNQNX_PRESSABLE_DISABLE_REMOVED_TARGET_FIX=1")
  set(expected_failure "remount press-in missing")
  set(control_marker "RNQNX_PRESSABLE_REMOVED_TARGET_CONTROL_REJECTED")
else()
  message(FATAL_ERROR "Unknown Pressable negative control: ${CONTROL}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    --unset=RNQNX_PRESSABLE_INTERCEPT_TEST
    --unset=RNQNX_PRESSABLE_DISABLE_REMOVED_TARGET_FIX
    "${control_env}"
    QT_QUICK_BACKEND=software ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
    "${PROBE_PATH}" -platform offscreen
  TIMEOUT 20
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result STREQUAL "1" OR NOT
    errors MATCHES "RNQNX_PRESSABLE_FAIL ${expected_failure}")
  message(FATAL_ERROR "Pressable control failed at wrong boundary: ${result}\n${output}\n${errors}")
endif()
if(output MATCHES "RNQNX_PRESSABLE_OK")
  message(FATAL_ERROR "Negative control falsely claimed Pressable delivery")
endif()
if("${output}\n${errors}" MATCHES
    "AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:")
  message(FATAL_ERROR "Pressable control emitted sanitizer diagnostics: ${output}\n${errors}")
endif()
message(STATUS "${control_marker}")
