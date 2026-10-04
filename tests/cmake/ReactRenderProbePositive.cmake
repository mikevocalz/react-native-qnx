# CTest PASS_REGULAR_EXPRESSION ignores normal nonzero exit codes. Check both
# exit and stdout explicitly, so an exception echoing the marker cannot pass.
execute_process(
  COMMAND "${PROBE}" -platform offscreen
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
  TIMEOUT 25)
if(NOT result STREQUAL "0" OR
   NOT output MATCHES "(^|\n)RNQNX_REACT_RENDER_OK platform=host bundled-react.*unmount=empty geometry-only transactions=[0-9]+" OR
   "${output}\n${error}" MATCHES "RNQNX_REACT_RENDER_ERROR")
  message(FATAL_ERROR "React render probe failed: ${result}\n${output}\n${error}")
endif()
message("${output}")
