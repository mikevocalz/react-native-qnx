include_guard(GLOBAL)

function(rnqnx_add_view_focus_test)
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING OR TARGET rnqnx_view_focus_host_test)
    return()
  endif()
  get_filename_component(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-view-focus.bundle.js")
  add_custom_command(OUTPUT "${bundle}"
    COMMAND "${root}/node_modules/.bin/metro" build
      ../../tests/fixtures/view-focus-probe.js --config metro.public.config.js
      --platform qnx --dev false --max-workers 2 --out "${bundle}"
    WORKING_DIRECTORY "${root}/apps/rn-playground"
    DEPENDS
      "${root}/tests/fixtures/view-focus-probe.js"
      "${root}/apps/rn-playground/metro.public.config.js"
      "${root}/apps/rn-playground/metro.config.js"
      "${root}/apps/rn-playground/babel.config.js"
      "${root}/packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js"
      "${root}/packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js"
      "${root}/packages/metro-config-qnx/public-entry.js"
      "${root}/pnpm-lock.yaml"
    VERBATIM)
  add_custom_target(rnqnx_view_focus_fixture DEPENDS "${bundle}")
  add_executable(rnqnx_view_focus_host_test "${root}/tests/cpp/QtViewFocusHostTest.cpp")
  target_link_libraries(rnqnx_view_focus_host_test PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_view_focus_host_test)
  add_dependencies(rnqnx_view_focus_host_test rnqnx_view_focus_fixture)
  target_compile_definitions(rnqnx_view_focus_host_test PRIVATE RNQNX_VIEW_FOCUS_BUNDLE_PATH="${bundle}")
  set_target_properties(rnqnx_view_focus_host_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    target_compile_options(rnqnx_view_focus_host_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(rnqnx_view_focus_host_test PRIVATE -fsanitize=address)
  endif()
  add_test(NAME rnqnx_view_focus_host_test COMMAND rnqnx_view_focus_host_test -platform offscreen)
  set_tests_properties(rnqnx_view_focus_host_test PROPERTIES TIMEOUT 30
    PASS_REGULAR_EXPRESSION "RNQNX_VIEW_FOCUS_OK"
    ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
endfunction()
