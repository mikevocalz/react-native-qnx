# Failures must exit nonzero AND must not emit a success marker. WILL_FAIL alone
# would also accept unrelated crashes, so check the diagnostic for each input.
file(MAKE_DIRECTORY "${WORK_DIR}")
file(WRITE "${WORK_DIR}/empty.js" "")
file(WRITE "${WORK_DIR}/throws.js" "throw new Error('probe-negative-fixture');\n")
file(WRITE "${WORK_DIR}/unregistered.js" "6 * 7;\n")
foreach(case IN ITEMS missing empty throws unregistered)
  if(case STREQUAL "missing")
    set(expected "bundle-open")
  elseif(case STREQUAL "empty")
    set(expected "bundle-read")
  elseif(case STREQUAL "throws")
    set(expected "bundle-evaluate")
  else()
    set(expected "bundle-registration")
  endif()
  execute_process(
    COMMAND "${PROBE}" -platform offscreen --bundle "${WORK_DIR}/${case}.js"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 10)
  if(NOT result MATCHES "^[1-9][0-9]*$" OR
     NOT error MATCHES "RNQNX_REACT_RENDER_ERROR ${expected}" OR
     "${output}\n${error}" MATCHES "RNQNX_REACT_RENDER_OK")
    message(FATAL_ERROR "${case} bundle was not rejected cleanly: ${result}\n${output}\n${error}")
  endif()
endforeach()
