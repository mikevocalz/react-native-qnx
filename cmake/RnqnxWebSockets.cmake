include_guard(GLOBAL)

option(RNQNX_ENABLE_WEBSOCKETS
  "Enable the experimental host Qt WebSockets backend (QNX acceptance pending)" OFF)

if(RNQNX_ENABLE_WEBSOCKETS)
  # QNX: Qt WebSockets cross-built from the public Qt 6.12.0 source
  # (scripts/qt-public/build-websockets-qnx.sh); size and build time are in
  # docs/build-qt-modules.md.
  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Network WebSockets)
  add_library(rnqnx_websocket_transport STATIC
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QtWebSocketTransport.cpp")
  target_include_directories(rnqnx_websocket_transport PUBLIC
    "${RNQNX_REACT_QT_ROOT}/vnext/include")
  target_link_libraries(rnqnx_websocket_transport PUBLIC
    Qt6::Core Qt6::Network Qt6::WebSockets)
  target_compile_definitions(rnqnx_websocket_transport PUBLIC QT_NO_KEYWORDS)
  rnqnx_enable_strict_warnings(rnqnx_websocket_transport)

  if(BUILD_TESTING)
    add_executable(qt_websocket_transport_host_test
      "${RNQNX_REACT_QT_ROOT}/tests/cpp/QtWebSocketTransportHostTest.cpp")
    target_link_libraries(qt_websocket_transport_host_test PRIVATE
      rnqnx_websocket_transport)
    rnqnx_enable_strict_warnings(qt_websocket_transport_host_test)
    if(RNQNX_ENABLE_HOST_SANITIZERS AND
       CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
      foreach(target rnqnx_websocket_transport qt_websocket_transport_host_test)
        target_compile_options(${target} PRIVATE
          -fsanitize=address,undefined -fno-sanitize-recover=undefined
          -fno-omit-frame-pointer)
        target_link_options(${target} PUBLIC -fsanitize=address,undefined)
      endforeach()
    endif()
    add_test(NAME qt_websocket_transport_host_test
      COMMAND qt_websocket_transport_host_test)
    set_tests_properties(qt_websocket_transport_host_test PROPERTIES
      TIMEOUT 30
      ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endif()
endif()

# Optional isolated Hermes/JSI contract tests do not require the Fabric renderer.
# The source revision and JSI ABI are still checked by the common Hermes helper.
option(RNQNX_BUILD_WEBSOCKET_JSI_TESTS
  "Build real Hermes/JSI tests for enabled and disabled WebSocketModule" OFF)
if(RNQNX_BUILD_WEBSOCKET_JSI_TESTS)
  if(NOT BUILD_TESTING OR NOT RNQNX_ENABLE_WEBSOCKETS)
    message(FATAL_ERROR
      "RNQNX_BUILD_WEBSOCKET_JSI_TESTS requires BUILD_TESTING and RNQNX_ENABLE_WEBSOCKETS")
  endif()
  set(RNQNX_NO_UPSTREAM_BUILD "rnqnx-not-a-cmake-project")
  include(${CMAKE_CURRENT_LIST_DIR}/dependencies/Hermes.cmake)
  foreach(mode enabled disabled)
    set(target qnx_websocket_module_${mode}_host_test)
    add_executable(${target}
      "${RNQNX_REACT_QT_ROOT}/tests/cpp/QnxWebSocketModuleHostTest.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxWebSocketModule.cpp"
      "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/react/nativemodule/core/ReactCommon/TurboModule.cpp")
    target_include_directories(${target} PRIVATE "${RNQNX_REACT_QT_ROOT}/vnext/include")
    target_include_directories(${target} SYSTEM PRIVATE
      "${RNQNX_REACT_NATIVE_DIR}/ReactCommon"
      "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi"
      "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/callinvoker"
      "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/react/nativemodule/core")
    target_link_libraries(${target} PRIVATE rnqnx_hermes Qt6::Core)
    target_compile_definitions(${target} PRIVATE QT_NO_KEYWORDS)
    if(mode STREQUAL "enabled")
      target_compile_definitions(${target} PRIVATE RNQNX_ENABLE_WEBSOCKETS)
      target_link_libraries(${target} PRIVATE rnqnx_websocket_transport)
    endif()
    rnqnx_enable_strict_warnings(${target})
    if(RNQNX_ENABLE_HOST_SANITIZERS AND
       CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined
        -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 30
      ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endforeach()
endif()
