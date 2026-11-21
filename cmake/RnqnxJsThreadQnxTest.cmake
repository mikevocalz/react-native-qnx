include_guard(GLOBAL)

# ADR-0021 on QNX. Cross-builds the JS-thread test so the QEMU VM can run it.
# What it proves is QNX VM evidence (software render, TCG emulation), never Pi
# 400 or release-board evidence.
#
# The QNX Hermes is the lean build and only runs bytecode, so the fixture
# scripts under tests/fixtures/js-thread are compiled with the pinned host
# hermesc. Supply it as RNQNX_JS_THREAD_TEST_HERMESC, as the Nitro Qt test does.
option(RNQNX_BUILD_JS_THREAD_QNX_TEST
  "Cross-build the ADR-0021 JS-thread test for the QNX VM" OFF)
set(RNQNX_JS_THREAD_TEST_HERMESC "" CACHE FILEPATH
  "Pinned host hermesc (hermes-compiler 260318099.0.5) for the JS-thread fixtures")

function(rnqnx_add_js_thread_qnx_test)
  if(NOT RNQNX_BUILD_JS_THREAD_QNX_TEST)
    return()
  endif()
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
    message(FATAL_ERROR "RNQNX_BUILD_JS_THREAD_QNX_TEST is for the QNX cross-build; "
      "the host runs qt_react_host_js_thread_host_test")
  endif()
  if(NOT RNQNX_ENABLE_WINDOW_CHROME)
    message(FATAL_ERROR "The JS-thread QNX test checks the title bar; set RNQNX_ENABLE_WINDOW_CHROME=ON")
  endif()
  if(NOT EXISTS "${RNQNX_JS_THREAD_TEST_HERMESC}")
    message(FATAL_ERROR "Supply RNQNX_JS_THREAD_TEST_HERMESC from the pinned hermes-compiler 260318099.0.5")
  endif()

  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  set(source "${root}/tests/fixtures/js-thread")
  set(output_directory "${CMAKE_BINARY_DIR}/js-thread-scripts")
  file(MAKE_DIRECTORY "${output_directory}")
  file(GLOB scripts CONFIGURE_DEPENDS "${source}/*.js")
  set(compiled "")
  foreach(script IN LISTS scripts)
    get_filename_component(name "${script}" NAME_WE)
    set(output "${output_directory}/${name}.hbc")
    add_custom_command(OUTPUT "${output}"
      COMMAND "${RNQNX_JS_THREAD_TEST_HERMESC}" -O -emit-binary -out "${output}" "${script}"
      DEPENDS "${script}" "${RNQNX_JS_THREAD_TEST_HERMESC}"
      VERBATIM)
    list(APPEND compiled "${output}")
  endforeach()
  add_custom_target(rnqnx_js_thread_scripts DEPENDS ${compiled})

  add_executable(qt_react_host_js_thread_qnx_test "${root}/tests/cpp/QtReactHostJsThreadHostTest.cpp")
  add_dependencies(qt_react_host_js_thread_qnx_test rnqnx_js_thread_scripts)
  target_link_libraries(qt_react_host_js_thread_qnx_test PRIVATE rnqnx_react_host)
  target_compile_definitions(qt_react_host_js_thread_qnx_test PRIVATE
    RNQNX_JS_THREAD_SCRIPT_DIR="${output_directory}"
    RNQNX_JS_THREAD_SCRIPT_EXT=".hbc")
  # What each build can reach. Libraries that refuse in JS-thread mode are
  # exercised only when the build links them.
  if(TARGET rnqnx_worklets)
    target_link_libraries(qt_react_host_js_thread_qnx_test PRIVATE rnqnx_worklets)
    target_compile_definitions(qt_react_host_js_thread_qnx_test PRIVATE RNQNX_JS_THREAD_TEST_WORKLETS)
  endif()
  if(TARGET rnqnx_reanimated)
    target_link_libraries(qt_react_host_js_thread_qnx_test PRIVATE rnqnx_reanimated)
    target_compile_definitions(qt_react_host_js_thread_qnx_test PRIVATE RNQNX_JS_THREAD_TEST_REANIMATED)
  endif()
  if(TARGET rnqnx_gesture_handler)
    target_link_libraries(qt_react_host_js_thread_qnx_test PRIVATE rnqnx_gesture_handler)
    target_compile_definitions(qt_react_host_js_thread_qnx_test PRIVATE RNQNX_JS_THREAD_TEST_GESTURE)
  endif()
  if(RNQNX_ENABLE_WEBSOCKETS)
    target_link_libraries(qt_react_host_js_thread_qnx_test PRIVATE Qt6::WebSockets)
    target_compile_definitions(qt_react_host_js_thread_qnx_test PRIVATE RNQNX_JS_THREAD_TEST_WEBSOCKET)
  endif()
  if(TARGET rnqnx_nitro_qt AND TARGET rnqnx_nitro_qnx)
    target_link_libraries(qt_react_host_js_thread_qnx_test PRIVATE rnqnx_nitro_qt rnqnx_nitro_qnx)
    target_compile_definitions(qt_react_host_js_thread_qnx_test PRIVATE RNQNX_JS_THREAD_TEST_NITRO)
    if(DEFINED RNQNX_NITRO_V1_COMPAT)
      target_compile_options(qt_react_host_js_thread_qnx_test PRIVATE "-include" "${RNQNX_NITRO_V1_COMPAT}")
    endif()
  endif()
  rnqnx_enable_strict_warnings(qt_react_host_js_thread_qnx_test)
  set_target_properties(qt_react_host_js_thread_qnx_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()
