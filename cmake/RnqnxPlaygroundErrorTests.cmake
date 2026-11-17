include_guard(GLOBAL)

function(rnqnx_add_playground_error_tests)
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING OR NOT TARGET rnqnx_playground_host)
    return()
  endif()
  foreach(kind IN ITEMS fatal soft)
    set(bundle "${CMAKE_BINARY_DIR}/rnqnx-error-control-${kind}.bundle.js")
    # Metro tracks the transitive platform/package graph; rerun it for each
    # requested build instead of trusting a partial list of JavaScript inputs.
    add_custom_target(rnqnx_playground_${kind}_bundle BYPRODUCTS "${bundle}"
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/.bin/metro" build
        "error-control-${kind}.js" --platform qnx --dev false --out "${bundle}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground"
      COMMENT "Bundling real playground ${kind}-error control"
      VERBATIM)
    add_dependencies(rnqnx_playground_host rnqnx_playground_${kind}_bundle)
    set(${kind}_bundle "${bundle}")
  endforeach()
  add_test(NAME rnqnx_playground_rejects_reported_fatal
    COMMAND "${CMAKE_COMMAND}"
      "-DPROBE=$<TARGET_FILE:rnqnx_playground_host>"
      "-DBUNDLE=${fatal_bundle}"
      "-DWORK_DIR=${CMAKE_BINARY_DIR}/playground-reported-fatal"
      -DEXPECT_FAILURE=RNQNX_TEST_MISSING_BOOTSTRAP
      -DEXPECT_BOUNDARY=BUNDLE
      "-DEXPECT_REPORT=[js fatal]"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/PlaygroundScreenshot.cmake")
  add_test(NAME rnqnx_playground_allows_reported_soft
    COMMAND "${CMAKE_COMMAND}"
      "-DPROBE=$<TARGET_FILE:rnqnx_playground_host>"
      "-DBUNDLE=${soft_bundle}"
      "-DWORK_DIR=${CMAKE_BINARY_DIR}/playground-reported-soft"
      "-DEXPECT_REPORT=[js error] Error: RNQNX_TEST_REPORTED_SOFT_ERROR"
      -DEXPECT_MESSAGE=RNQNX_TEST_CAUGHT_ERROR_CONTINUED
      -DEXPECT_MOUNT=RNQNX_TEST_ERROR_CONTROL_MOUNTED
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/PlaygroundScreenshot.cmake")
  set_tests_properties(rnqnx_playground_rejects_reported_fatal
    rnqnx_playground_allows_reported_soft PROPERTIES TIMEOUT 30)
endfunction()
