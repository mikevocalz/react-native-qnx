include_guard(GLOBAL)

function(rnqnx_add_consumer_input_tests)
  # Exercise the production startup with either value of the lifecycle option.
  # A separate target run is needed to qualify physical QNX pointer delivery.
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-consumer-input.bundle.js")
  add_custom_command(OUTPUT "${bundle}"
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/.bin/metro" build
      consumer-input-probe.js --platform qnx --dev false --max-workers 2 --out "${bundle}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/consumer-input-probe.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/metro.config.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/babel.config.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/metro-config-qnx/src/index.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/pnpm-lock.yaml"
    VERBATIM)
  add_custom_target(rnqnx_consumer_input_fixture DEPENDS "${bundle}")
  add_executable(rnqnx_consumer_input_test
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/cpp/QtConsumerWindowInputTest.cpp")
  target_link_libraries(rnqnx_consumer_input_test PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_consumer_input_test)
  add_dependencies(rnqnx_consumer_input_test rnqnx_consumer_input_fixture)
  target_compile_definitions(rnqnx_consumer_input_test PRIVATE RNQNX_PLAYGROUND_BUNDLE_PATH="${bundle}")
  set_target_properties(rnqnx_consumer_input_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    target_compile_options(rnqnx_consumer_input_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(rnqnx_consumer_input_test PRIVATE -fsanitize=address)
  endif()
  add_test(NAME rnqnx_consumer_window_input
    COMMAND rnqnx_consumer_input_test -platform offscreen)
  set(input_tests rnqnx_consumer_window_input rnqnx_consumer_window_rejects_interception)
  if(RNQNX_ENABLE_DESKTOP_WINDOW)
    add_test(NAME rnqnx_consumer_desktop_window_input
      COMMAND rnqnx_consumer_input_test -platform offscreen --desktop)
    set_tests_properties(rnqnx_consumer_desktop_window_input PROPERTIES
      PASS_REGULAR_EXPRESSION "RNQNX_CONSUMER_INPUT_OK window padding text repeat disabled tab modal after-close scrolled-portal")
    list(APPEND input_tests rnqnx_consumer_desktop_window_input)
  endif()
  set_tests_properties(rnqnx_consumer_window_input PROPERTIES
    PASS_REGULAR_EXPRESSION "RNQNX_CONSUMER_INPUT_OK window padding text repeat disabled tab modal after-close")
  add_test(NAME rnqnx_consumer_window_rejects_interception
    COMMAND "${CMAKE_COMMAND}"
      "-DPROBE=$<TARGET_FILE:rnqnx_consumer_input_test>"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/cmake/ConsumerWindowInputNegative.cmake")
  set_tests_properties(${input_tests} PROPERTIES
    TIMEOUT 20
    ENVIRONMENT "QT_QUICK_BACKEND=software;QSG_RHI_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
endfunction()
