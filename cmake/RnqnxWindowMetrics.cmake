include_guard(GLOBAL)

function(rnqnx_add_window_metrics_test)
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-window-metrics.bundle.js")
  add_custom_command(OUTPUT "${bundle}"
    COMMAND "${root}/node_modules/.bin/metro" build
      ../../tests/fixtures/window-metrics-probe.js --config metro.public.config.js
      --platform qnx --dev false --max-workers 2 --out "${bundle}"
    WORKING_DIRECTORY "${root}/apps/rn-playground"
    DEPENDS
      "${root}/tests/fixtures/window-metrics-probe.js"
      "${root}/apps/rn-playground/metro.public.config.js"
      "${root}/apps/rn-playground/metro.config.js"
      "${root}/apps/rn-playground/babel.config.js"
      "${root}/packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js"
      "${root}/packages/metro-config-qnx/public-entry.js"
      "${root}/pnpm-lock.yaml"
    VERBATIM)
  add_custom_target(rnqnx_window_metrics_fixture DEPENDS "${bundle}")
  add_executable(rnqnx_window_metrics_test "${root}/tests/cpp/QtWindowMetricsTest.cpp")
  target_link_libraries(rnqnx_window_metrics_test PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_window_metrics_test)
  add_dependencies(rnqnx_window_metrics_test rnqnx_window_metrics_fixture)
  target_compile_definitions(rnqnx_window_metrics_test PRIVATE RNQNX_WINDOW_METRICS_BUNDLE_PATH="${bundle}")
  set_target_properties(rnqnx_window_metrics_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    target_compile_options(rnqnx_window_metrics_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(rnqnx_window_metrics_test PRIVATE -fsanitize=address)
  endif()
  foreach(scale 1 1.25 2)
    string(REPLACE "." "_" suffix "${scale}")
    add_test(NAME rnqnx_window_metrics_scale_${suffix}
      COMMAND rnqnx_window_metrics_test -platform offscreen)
    set_tests_properties(rnqnx_window_metrics_scale_${suffix} PROPERTIES TIMEOUT 20
      PASS_REGULAR_EXPRESSION "RNQNX_WINDOW_METRICS_OK"
      ENVIRONMENT "QT_QUICK_BACKEND=software;QT_SCALE_FACTOR=${scale};RNQNX_EXPECTED_SCALE=${scale};ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endforeach()
endfunction()
