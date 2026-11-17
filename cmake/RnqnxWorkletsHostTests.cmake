include_guard(GLOBAL)

# This application links all three optional native targets and imports their
# actual Metro packages. Building static archives alone cannot prove bootstrap.
function(rnqnx_add_worklets_host_tests)
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING OR
     NOT RNQNX_ENABLE_WORKLETS OR NOT RNQNX_ENABLE_REANIMATED OR NOT RNQNX_ENABLE_GESTURE_HANDLER)
    return()
  endif()
  foreach(target IN ITEMS rnqnx_react_host rnqnx_worklets rnqnx_reanimated rnqnx_gesture_handler)
    if(NOT TARGET ${target})
      message(FATAL_ERROR "Worklets integration tests require ${target}")
    endif()
  endforeach()

  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-worklets-integration.bundle.js")
  # Metro owns the transitive JavaScript graph. Run it for each requested build
  # so platform overlays or installed-package edits cannot leave a stale bundle
  # behind an otherwise current native executable. Metro retains its own cache.
  add_custom_target(rnqnx_worklets_integration_bundle BYPRODUCTS "${bundle}"
    COMMAND "${RNQNX_REACT_QT_ROOT}/node_modules/.bin/metro" build
      worklets-integration.js --platform qnx --dev false --out "${bundle}"
    WORKING_DIRECTORY "${RNQNX_REACT_QT_ROOT}/apps/rn-playground"
    DEPENDS
      "${RNQNX_REACT_QT_ROOT}/apps/rn-playground/worklets-integration.js"
      "${RNQNX_REACT_QT_ROOT}/apps/rn-playground/metro.config.js"
      "${RNQNX_REACT_QT_ROOT}/apps/rn-playground/babel.config.js"
      "${RNQNX_REACT_QT_ROOT}/packages/metro-config-qnx/src/index.js"
      "${RNQNX_REACT_QT_ROOT}/pnpm-lock.yaml"
    COMMENT "Bundling real Worklets/Reanimated/Gesture Handler host fixture"
    VERBATIM)
  add_executable(rnqnx_worklets_integration_host_test
    "${RNQNX_REACT_QT_ROOT}/tests/cpp/WorkletsIntegrationHostTest.cpp")
  add_dependencies(rnqnx_worklets_integration_host_test rnqnx_worklets_integration_bundle)
  target_compile_definitions(rnqnx_worklets_integration_host_test PRIVATE
    RNQNX_WORKLETS_INTEGRATION_BUNDLE_PATH="${bundle}")
  target_link_libraries(rnqnx_worklets_integration_host_test PRIVATE
    rnqnx_react_host rnqnx_worklets rnqnx_reanimated rnqnx_gesture_handler)
  set_target_properties(rnqnx_worklets_integration_host_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  rnqnx_enable_strict_warnings(rnqnx_worklets_integration_host_test)
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    target_compile_options(rnqnx_worklets_integration_host_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(rnqnx_worklets_integration_host_test PRIVATE -fsanitize=address)
  endif()
  add_test(NAME rnqnx_worklets_integration_host_test
    COMMAND rnqnx_worklets_integration_host_test -platform offscreen)
  add_test(NAME rnqnx_worklets_rejects_missing_bootstrap
    COMMAND rnqnx_worklets_integration_host_test --omit-worklets -platform offscreen)
  set_tests_properties(rnqnx_worklets_integration_host_test rnqnx_worklets_rejects_missing_bootstrap
    PROPERTIES TIMEOUT 60 ENVIRONMENT
      "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1")
  set_property(TEST rnqnx_worklets_integration_host_test APPEND PROPERTY ENVIRONMENT
    "RNQNX_WORKLETS_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/evidence/worklets-integration")
endfunction()
