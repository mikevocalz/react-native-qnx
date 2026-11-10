include_guard(GLOBAL)

option(RNQNX_ENABLE_DEV_RUNTIME
  "Enable host-only Metro development sessions (not QNX-qualified)" OFF)

if(RNQNX_ENABLE_DEV_RUNTIME)
  if(CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME MATCHES "^(Darwin|Linux)$")
    message(FATAL_ERROR "Development runtime is native Darwin/Linux host-only; QNX is not qualified")
  endif()
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Development runtime requires a separate Debug build tree; never enable in release artifacts")
  endif()
  if(NOT RNQNX_ENABLE_WEBSOCKETS)
    message(FATAL_ERROR "Development runtime requires RNQNX_ENABLE_WEBSOCKETS=ON")
  endif()
endif()

function(rnqnx_configure_dev_runtime)
  target_compile_definitions(rnqnx_react_host PUBLIC
    RNQNX_ENABLE_DEV_RUNTIME=$<BOOL:${RNQNX_ENABLE_DEV_RUNTIME}>)
  if(NOT RNQNX_ENABLE_DEV_RUNTIME)
    return()
  endif()
  if(NOT TARGET hermesvm OR NOT TARGET rnqnx_websocket_transport)
    message(FATAL_ERROR "Development sessions require the pinned full/compiler-enabled host Hermes and Qt WebSockets")
  endif()
  target_sources(rnqnx_react_host PRIVATE
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/DevSession.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxDevSettings.cpp")
  if(BUILD_TESTING)
    add_executable(rnqnx_dev_session_host_test
      "${RNQNX_REACT_QT_ROOT}/tests/cpp/DevSessionHostTest.cpp")
    target_link_libraries(rnqnx_dev_session_host_test PRIVATE rnqnx_react_host)
    rnqnx_enable_strict_warnings(rnqnx_dev_session_host_test)
    if(RNQNX_ENABLE_HOST_SANITIZERS AND
       CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
      target_compile_options(rnqnx_dev_session_host_test PRIVATE
        -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
      target_link_options(rnqnx_dev_session_host_test PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME rnqnx_dev_session_host_test COMMAND rnqnx_dev_session_host_test -platform offscreen)
    set_tests_properties(rnqnx_dev_session_host_test PROPERTIES TIMEOUT 30
      ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endif()
endfunction()
