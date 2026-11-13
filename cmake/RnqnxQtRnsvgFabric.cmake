include_guard(GLOBAL)

function(rnqnx_add_qt_rnsvg_fabric_test)
  if(NOT RNQNX_ENABLE_QT_RNSVG OR NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-rnsvg.bundle.js")
  add_custom_command(OUTPUT "${bundle}"
    COMMAND "${RNQNX_REACT_QT_ROOT}/node_modules/.bin/metro" build rnsvg-fixture.js
      --platform qnx --dev false --out "${bundle}"
    WORKING_DIRECTORY "${RNQNX_REACT_QT_ROOT}/apps/rn-playground"
    DEPENDS
      "${RNQNX_REACT_QT_ROOT}/apps/rn-playground/rnsvg-fixture.js"
      "${RNQNX_REACT_QT_ROOT}/apps/rn-playground/metro.config.js"
    VERBATIM)
  add_custom_target(rnqnx_rnsvg_bundle DEPENDS "${bundle}")
  add_executable(rnqnx_qt_rnsvg_fabric_test
    "${RNQNX_REACT_QT_ROOT}/tests/native/qt-rnsvg-fabric/RnsvgFabricTest.cpp")
  add_dependencies(rnqnx_qt_rnsvg_fabric_test rnqnx_rnsvg_bundle)
  target_link_libraries(rnqnx_qt_rnsvg_fabric_test PRIVATE rnqnx_react_host)
  target_compile_definitions(rnqnx_qt_rnsvg_fabric_test PRIVATE RNQNX_RNSVG_BUNDLE_PATH="${bundle}")
  rnqnx_enable_strict_warnings(rnqnx_qt_rnsvg_fabric_test)
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rnqnx_qt_rnsvg_fabric_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(rnqnx_qt_rnsvg_fabric_test PRIVATE -fsanitize=address)
  endif()
  add_test(NAME rnqnx_qt_rnsvg_fabric_test COMMAND rnqnx_qt_rnsvg_fabric_test -platform offscreen)
  set_tests_properties(rnqnx_qt_rnsvg_fabric_test PROPERTIES TIMEOUT 60
    ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1")
endfunction()

# <Image> from a data URI and from a bundled asset, with onLoad events.
function(rnqnx_add_qt_rnsvg_image_fabric_test)
  if(NOT RNQNX_ENABLE_QT_RNSVG OR NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  set(image_bundle "${CMAKE_BINARY_DIR}/rnqnx-rnsvg-image.bundle.js")
  set(image_dir "${RNQNX_REACT_QT_ROOT}/tests/native/qt-rnsvg-fabric")
  add_custom_command(OUTPUT "${image_bundle}"
    COMMAND "${RNQNX_REACT_QT_ROOT}/node_modules/.bin/metro" build
      "${image_dir}/rnsvg-image-fixture.js" --platform qnx --dev false --out "${image_bundle}"
    WORKING_DIRECTORY "${RNQNX_REACT_QT_ROOT}/apps/rn-playground"
    DEPENDS "${image_dir}/rnsvg-image-fixture.js" "${image_dir}/rnsvg-image-asset.png"
      "${RNQNX_REACT_QT_ROOT}/apps/rn-playground/metro.config.js" VERBATIM)
  add_custom_target(rnqnx_rnsvg_image_bundle DEPENDS "${image_bundle}")
  add_executable(rnqnx_qt_rnsvg_image_fabric_test "${image_dir}/RnsvgImageFabricTest.cpp")
  add_dependencies(rnqnx_qt_rnsvg_image_fabric_test rnqnx_rnsvg_image_bundle)
  target_link_libraries(rnqnx_qt_rnsvg_image_fabric_test PRIVATE rnqnx_react_host)
  target_compile_definitions(rnqnx_qt_rnsvg_image_fabric_test PRIVATE
    RNQNX_RNSVG_IMAGE_BUNDLE_PATH="${image_bundle}"
    RNQNX_RNSVG_IMAGE_ASSET_PATH="${image_dir}/rnsvg-image-asset.png")
  rnqnx_enable_strict_warnings(rnqnx_qt_rnsvg_image_fabric_test)
  add_test(NAME rnqnx_qt_rnsvg_image_fabric_test COMMAND rnqnx_qt_rnsvg_image_fabric_test -platform offscreen)
  set_tests_properties(rnqnx_qt_rnsvg_image_fabric_test PROPERTIES TIMEOUT 60
    ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1")
endfunction()

# The fixture corpus through react-native-svg's own SvgXml, the real Fabric
# props and the bridge, against the same resvg goldens as RnsvgFixturesTest.
function(rnqnx_add_qt_rnsvg_corpus_test)
  if(NOT RNQNX_ENABLE_QT_RNSVG OR NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  find_program(RNQNX_NODE node REQUIRED)
  set(root "${RNQNX_REACT_QT_ROOT}")
  set(gen "${root}/apps/rn-playground/rnsvg-corpus-generated")
  set(layout "${CMAKE_BINARY_DIR}/rnqnx-rnsvg-corpus-layout.json")
  set(bundle "${CMAKE_BINARY_DIR}/rnqnx-rnsvg-corpus.bundle.js")
  file(GLOB fixture_svgs CONFIGURE_DEPENDS "${root}/tests/fixtures/rnsvg/svg/*.svg")
  add_custom_command(OUTPUT "${gen}/rnsvg-corpus-data.js" "${layout}"
    COMMAND "${RNQNX_NODE}" "${root}/scripts/rnsvg-reference.mjs" --corpus "${gen}" --layout "${layout}"
    DEPENDS ${fixture_svgs} "${root}/tests/fixtures/rnsvg/manifest.json" "${root}/scripts/rnsvg-reference.mjs"
    VERBATIM)
  add_custom_command(OUTPUT "${bundle}"
    COMMAND "${root}/node_modules/.bin/metro" build rnsvg-corpus.js --platform qnx --dev false --out "${bundle}"
    WORKING_DIRECTORY "${root}/apps/rn-playground"
    DEPENDS "${root}/apps/rn-playground/rnsvg-corpus.js" "${gen}/rnsvg-corpus-data.js"
            "${root}/apps/rn-playground/metro.config.js"
    VERBATIM)
  add_custom_target(rnqnx_rnsvg_corpus_bundle DEPENDS "${bundle}")
  file(GLOB support CONFIGURE_DEPENDS "${root}/tests/native/qt-rnsvg/support/*.cpp")
  add_executable(rnqnx_qt_rnsvg_corpus_test
    "${root}/tests/native/qt-rnsvg-fabric/RnsvgCorpusFabricTest.cpp" ${support})
  add_dependencies(rnqnx_qt_rnsvg_corpus_test rnqnx_rnsvg_corpus_bundle)
  target_link_libraries(rnqnx_qt_rnsvg_corpus_test PRIVATE rnqnx_react_host)
  target_include_directories(rnqnx_qt_rnsvg_corpus_test PRIVATE "${root}/native/qt/rnsvg")
  target_compile_definitions(rnqnx_qt_rnsvg_corpus_test PRIVATE
    RNQNX_RNSVG_BUNDLE_PATH="${bundle}" RNQNX_RNSVG_CORPUS_BUNDLE_PATH="${bundle}"
    RNQNX_RNSVG_CORPUS_LAYOUT="${layout}" RNQNX_RNSVG_FIXTURE_DIR="${root}/tests/fixtures/rnsvg")
  rnqnx_enable_strict_warnings(rnqnx_qt_rnsvg_corpus_test)
  # Independent Hermes/Fabric lifetimes bound corpus memory and isolate slow
  # scenes in Debug CI. Every batch belongs to exactly one of these four shards.
  foreach(shard RANGE 0 3)
    set(test_name rnqnx_qt_rnsvg_corpus_test)
    if(shard GREATER 0)
      string(APPEND test_name "_${shard}")
    endif()
    add_test(NAME ${test_name} COMMAND rnqnx_qt_rnsvg_corpus_test
      -platform offscreen --shard-index ${shard} --shard-count 4)
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 300
      ENVIRONMENT "QT_QUICK_BACKEND=software")
  endforeach()
endfunction()

# Hostile numbers through the bridge's value converters (no JS runtime).
function(rnqnx_add_qt_rnsvg_values_test)
  if(NOT RNQNX_ENABLE_QT_RNSVG OR NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  set(root "${RNQNX_REACT_QT_ROOT}")
  add_executable(rnqnx_qt_rnsvg_values_test "${root}/tests/native/qt-rnsvg-fabric/RnsvgBridgeValuesTest.cpp")
  target_link_libraries(rnqnx_qt_rnsvg_values_test PRIVATE rnqnx_fabric_adapter)
  target_include_directories(rnqnx_qt_rnsvg_values_test PRIVATE "${root}/vnext/src/fabric" "${root}/native/qt/rnsvg")
  rnqnx_enable_strict_warnings(rnqnx_qt_rnsvg_values_test)
  add_test(NAME rnqnx_qt_rnsvg_values_test COMMAND rnqnx_qt_rnsvg_values_test)
  set_tests_properties(rnqnx_qt_rnsvg_values_test PROPERTIES TIMEOUT 60)
endfunction()
