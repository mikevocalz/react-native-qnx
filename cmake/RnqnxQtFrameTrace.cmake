include_guard(GLOBAL)

function(rnqnx_add_qt_frame_trace)
  if(NOT RNQNX_ENABLE_FRAME_TRACE OR NOT TARGET Qt6::Quick)
    return()
  endif()
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  add_library(rnqnx_qt_frame_trace STATIC "${root}/vnext/src/runtime/QtFrameTrace.cpp")
  target_link_libraries(rnqnx_qt_frame_trace PUBLIC rnqnx_runtime_trace Qt6::Quick)
  rnqnx_enable_strict_warnings(rnqnx_qt_frame_trace)
  # Every executable built from the playground main links the React host, so
  # the observer travels with it.
  if(TARGET rnqnx_react_host)
    target_link_libraries(rnqnx_react_host PUBLIC rnqnx_qt_frame_trace)
  endif()
  if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    # The test compiles the observer itself so it runs under the sanitizers
    # without forcing them onto the app targets that link the library.
    add_executable(rnqnx_qt_frame_trace_test "${root}/tests/cpp/QtFrameTraceTest.cpp"
      "${root}/vnext/src/runtime/QtFrameTrace.cpp")
    target_link_libraries(rnqnx_qt_frame_trace_test PRIVATE rnqnx_runtime_trace Qt6::Quick)
    rnqnx_enable_strict_warnings(rnqnx_qt_frame_trace_test)
    if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      target_compile_options(rnqnx_qt_frame_trace_test PRIVATE -fsanitize=address,undefined
        -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
      target_link_options(rnqnx_qt_frame_trace_test PRIVATE -fsanitize=address,undefined)
    endif()
    set_target_properties(rnqnx_qt_frame_trace_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME rnqnx_qt_frame_trace_test COMMAND rnqnx_qt_frame_trace_test -platform offscreen)
    set_tests_properties(rnqnx_qt_frame_trace_test PROPERTIES TIMEOUT 15
      ENVIRONMENT "QT_QUICK_BACKEND=software;QSG_RENDER_LOOP=basic;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endif()
endfunction()
