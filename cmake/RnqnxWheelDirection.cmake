include_guard(GLOBAL)

function(rnqnx_add_wheel_direction_test)
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING OR CMAKE_SYSTEM_NAME STREQUAL "QNX"
      OR TARGET rnqnx_wheel_direction_test)
    return()
  endif()
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  add_executable(rnqnx_wheel_direction_test
    "${root}/tests/cpp/QtWheelDirectionTest.cpp"
    "${root}/vnext/src/views/qt/QtWheelDirectionFilter.cpp")
  target_include_directories(rnqnx_wheel_direction_test PRIVATE "${root}/vnext/include")
  target_link_libraries(rnqnx_wheel_direction_test PRIVATE Qt6::Quick Qt6::Qml Qt6::Gui)
  target_compile_definitions(rnqnx_wheel_direction_test PRIVATE QT_NO_KEYWORDS)
  rnqnx_enable_strict_warnings(rnqnx_wheel_direction_test)
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    target_compile_options(rnqnx_wheel_direction_test PRIVATE
      -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(rnqnx_wheel_direction_test PRIVATE -fsanitize=address,undefined)
  endif()
  set_target_properties(rnqnx_wheel_direction_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  add_test(NAME rnqnx_wheel_direction
    COMMAND $<TARGET_FILE:rnqnx_wheel_direction_test> -platform offscreen)
  set_tests_properties(rnqnx_wheel_direction PROPERTIES TIMEOUT 15
    ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1"
    PASS_REGULAR_EXPRESSION "RNQNX_WHEEL_DIRECTION_OK default metadata acceptance lifetime mouse touch flickable")
endfunction()
