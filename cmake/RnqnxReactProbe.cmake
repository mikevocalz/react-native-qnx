include_guard(GLOBAL)

# Shared implementation: the QNX binary and host CTest run the same source and
# the same production Metro fixture. Metro runs on the build host in both cases.
function(rnqnx_add_react_render_probe target)
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-react-probe.bundle.js")
  add_custom_command(
    OUTPUT "${bundle}"
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/.bin/metro" build
            react-probe.js --platform qnx --dev false --out "${bundle}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/react-probe.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/metro.config.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/babel.config.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/metro-config-qnx/src/index.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/react-native-qnx/Libraries/Utilities/Platform.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/pnpm-lock.yaml"
    COMMENT "Bundling real React render probe for qnx"
    VERBATIM
  )
  add_custom_target(${target}_bundle DEPENDS "${bundle}")
  add_executable(${target} vnext/probes/react-qnx/main.cpp)
  target_link_libraries(${target} PRIVATE rnqnx_react_host Qt6::Quick Qt6::Qml Qt6::Gui)
  rnqnx_enable_strict_warnings(${target})
  add_dependencies(${target} ${target}_bundle)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

  if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
    target_compile_definitions(${target} PRIVATE
      RNQNX_REACT_PROBE_BUNDLE_PATH="${bundle}")
    add_test(NAME ${target}
      COMMAND "${CMAKE_COMMAND}"
        "-DPROBE=$<TARGET_FILE:${target}>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/ReactRenderProbePositive.cmake")
    set_tests_properties(${target} PROPERTIES TIMEOUT 30)
    add_test(NAME ${target}_rejects_bad_bundles
      COMMAND "${CMAKE_COMMAND}"
        "-DPROBE=$<TARGET_FILE:${target}>"
        "-DWORK_DIR=${CMAKE_BINARY_DIR}/react-probe-negative"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/ReactRenderProbeNegative.cmake")
    set_tests_properties(${target}_rejects_bad_bundles PROPERTIES TIMEOUT 30)
  endif()
endfunction()
