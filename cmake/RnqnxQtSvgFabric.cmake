include_guard(GLOBAL)

function(rnqnx_add_qt_svg_fabric_test)
  if(NOT RNQNX_ENABLE_QT_SVG OR NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-qt-svg.bundle.js")
  add_custom_command(OUTPUT "${bundle}"
    COMMAND "${RNQNX_REACT_QT_ROOT}/node_modules/.bin/metro" build qt-svg-fixture.js
      --platform qnx --dev false --out "${bundle}"
    WORKING_DIRECTORY "${RNQNX_REACT_QT_ROOT}/apps/rn-playground"
    DEPENDS
      "${RNQNX_REACT_QT_ROOT}/apps/rn-playground/qt-svg-fixture.js"
      "${RNQNX_REACT_QT_ROOT}/apps/rn-playground/metro.config.js"
      "${RNQNX_REACT_QT_ROOT}/packages/react-native-qnx/qt/index.js"
      "${RNQNX_REACT_QT_ROOT}/packages/react-native-qnx/qt/QtSvgDocumentNativeComponent.ts"
    VERBATIM)
  add_custom_target(rnqnx_qt_svg_bundle DEPENDS "${bundle}")
  add_executable(rnqnx_qt_svg_fabric_test
    "${RNQNX_REACT_QT_ROOT}/tests/native/qt-svg-fabric/SvgFabricTest.cpp")
  add_dependencies(rnqnx_qt_svg_fabric_test rnqnx_qt_svg_bundle)
  target_link_libraries(rnqnx_qt_svg_fabric_test PRIVATE rnqnx_react_host)
  target_compile_definitions(rnqnx_qt_svg_fabric_test PRIVATE RNQNX_QT_SVG_BUNDLE_PATH="${bundle}")
  rnqnx_enable_strict_warnings(rnqnx_qt_svg_fabric_test)
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rnqnx_qt_svg_fabric_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(rnqnx_qt_svg_fabric_test PRIVATE -fsanitize=address)
  endif()
  add_test(NAME rnqnx_qt_svg_fabric_test COMMAND rnqnx_qt_svg_fabric_test -platform offscreen)
  set_tests_properties(rnqnx_qt_svg_fabric_test PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1")
endfunction()
