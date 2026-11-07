if(NOT EXISTS "${PROBE}" OR NOT EXISTS "${BUNDLE}")
  message(FATAL_ERROR "Missing playground executable or bundle")
endif()
file(MAKE_DIRECTORY "${WORK_DIR}")
set(screenshot "${WORK_DIR}/frame.png")
file(REMOVE "${screenshot}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    QT_QUICK_BACKEND=software ASAN_OPTIONS=detect_leaks=0
    "${PROBE}" -platform offscreen --bundle "${BUNDLE}"
    --screenshot "${screenshot}"
  TIMEOUT 20
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if("${output}\n${errors}" MATCHES
    "AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:")
  message(FATAL_ERROR "Playground emitted sanitizer diagnostics: ${output}\n${errors}")
endif()
if(EXPECT_FAILURE)
  if(NOT result STREQUAL "1" OR NOT errors MATCHES
      "RNQNX_PLAYGROUND_RENDER_FAILED ${EXPECT_FAILURE}")
    message(FATAL_ERROR "Application failure control failed at wrong boundary: ${result}\n${output}\n${errors}")
  endif()
  if(EXISTS "${screenshot}" OR output MATCHES "RNQNX_PLAYGROUND_SCREENSHOT")
    message(FATAL_ERROR "Failed application falsely produced a success screenshot")
  endif()
  message(STATUS "RNQNX_PLAYGROUND_APPLICATION_FAILURE_REJECTED ${EXPECT_FAILURE}")
else()
  if(NOT result STREQUAL "0" OR NOT EXISTS "${screenshot}" OR NOT
      output MATCHES "RNQNX_PLAYGROUND_SCREENSHOT .* 640x480")
    message(FATAL_ERROR "Playground screenshot failed: ${result}\n${output}\n${errors}")
  endif()
  file(READ "${screenshot}" signature OFFSET 0 LIMIT 8 HEX)
  if(NOT signature STREQUAL "89504e470d0a1a0a")
    message(FATAL_ERROR "Playground artifact is not PNG")
  endif()
  message(STATUS "RNQNX_PLAYGROUND_SCREENSHOT_ARTIFACT_OK")
endif()
