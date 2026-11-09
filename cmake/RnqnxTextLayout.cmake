include_guard(GLOBAL)

function(rnqnx_add_text_layout_test)
  if(TARGET rnqnx_text_layout_host_test OR NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  get_filename_component(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  add_executable(rnqnx_text_layout_host_test "${root}/tests/cpp/QtTextLayoutHostTest.cpp")
  target_link_libraries(rnqnx_text_layout_host_test PRIVATE rnqnx_fabric_adapter)
  rnqnx_enable_strict_warnings(rnqnx_text_layout_host_test)
  set_target_properties(rnqnx_text_layout_host_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    target_compile_options(rnqnx_text_layout_host_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(rnqnx_text_layout_host_test PRIVATE -fsanitize=address)
  endif()
  add_test(NAME rnqnx_text_layout_host_test
    COMMAND rnqnx_text_layout_host_test -platform offscreen)
  set_tests_properties(rnqnx_text_layout_host_test PROPERTIES TIMEOUT 20
    PASS_REGULAR_EXPRESSION "RNQNX_QT_TEXT_LAYOUT_OK"
    ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
endfunction()
