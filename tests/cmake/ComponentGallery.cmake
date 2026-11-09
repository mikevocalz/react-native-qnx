# A rendered page can survive a fatal React callback. Reject those diagnostics
# as well as process failure and an incomplete real-window interaction sequence.
set(arguments -platform offscreen --bundle "${BUNDLE}"
  --app-id "${APP_ID}" --app-name "${APP_NAME}" --component-name "${COMPONENT_NAME}")
if(DESKTOP)
  list(APPEND arguments --desktop)
endif()
file(MAKE_DIRECTORY "${ARTIFACTS}")
# Reused build directories must not present an earlier run's screenshots as
# evidence from this run. Remove only files this runner and driver own.
file(REMOVE "${ARTIFACTS}/run.log" "${ARTIFACTS}/receipt.json")
foreach(snapshot IN ITEMS
    failure
    01-components-initial
    02-documents
    03-sections
    04-controls
    05-adaptive
    06-limits
    07-modal
    08-components-retained-state
    09-adaptive-minimum
    10-desktop-gallery
    gallery-expanded
    gallery-compact
    gallery-narrow
    gallery-short
    gallery-dialog-short)
  file(REMOVE "${ARTIFACTS}/${snapshot}.png" "${ARTIFACTS}/${snapshot}.json")
endforeach()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env "RNQNX_GALLERY_SMOKE_ARTIFACTS=${ARTIFACTS}"
    "${PROBE}" ${arguments}
  # The adaptive driver adds resize/caret checks and visits every destination
  # again through the minimum-window menu. Individual steps retain an 8s bound.
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 150)
set(log "${output}\n${errors}")
file(WRITE "${ARTIFACTS}/run.log" "${log}")
file(SHA256 "${BUNDLE}" bundle_sha)
file(SHA256 "${IMAGE}" image_sha)
file(SHA256 "${PROBE}" binary_sha)
set(passed true)
if(NOT result STREQUAL "0" OR NOT output MATCHES "RNQNX_GALLERY_INTERACTIONS_COMPLETE" OR
   log MATCHES "\\[js (fatal|task error|error)\\]|QML.*Image:.*(Cannot open|Error decoding)|RNQNX_GALLERY_(SMOKE|LAYOUT)_FAIL|AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:")
  set(passed false)
endif()
file(WRITE "${ARTIFACTS}/receipt.json"
  "{\n  \"schemaVersion\": 1,\n  \"hostOnly\": true,\n  \"overallPassed\": ${passed},\n  \"bundleSha256\": \"${bundle_sha}\",\n  \"imageSha256\": \"${image_sha}\",\n  \"binarySha256\": \"${binary_sha}\"\n}\n")
if(NOT passed)
  message(FATAL_ERROR "Actual component gallery failed: ${result}\n${log}")
endif()
message(STATUS "RNQNX_COMPONENT_GALLERY_OK ${ARTIFACTS}")
