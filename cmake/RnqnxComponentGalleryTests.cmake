include_guard(GLOBAL)

function(rnqnx_add_component_gallery_tests)
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  set(fixture_dir "${CMAKE_BINARY_DIR}/component-gallery-fixture")
  set(bundle "${fixture_dir}/app.bundle.js")
  set(image "${fixture_dir}/assets/assets/icon.png")
  # Public-entry and override changes must invalidate the actual app bundle.
  file(GLOB_RECURSE gallery_js CONFIGURE_DEPENDS
    "${root}/apps/component-conformance/*.js"
    "${root}/packages/react-native-qnx/adaptive/*.js"
    "${root}/packages/react-native-qnx/Libraries/*.js"
    "${root}/packages/metro-config-qnx/src/*.js")
  add_custom_command(OUTPUT "${bundle}" "${image}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${fixture_dir}/assets/assets"
    COMMAND "${root}/node_modules/.bin/metro" build index.js
      --platform qnx --dev false --max-workers 2 --out "${bundle}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${root}/apps/component-conformance/assets/icon.png" "${image}"
    WORKING_DIRECTORY "${root}/apps/component-conformance"
    DEPENDS ${gallery_js}
      "${root}/packages/metro-config-qnx/public-entry.js"
      "${root}/apps/component-conformance/assets/icon.png"
      "${root}/apps/component-conformance/app.json"
      "${root}/pnpm-lock.yaml"
    VERBATIM)
  add_custom_target(rnqnx_component_gallery_fixture DEPENDS "${bundle}" "${image}")
  add_executable(rnqnx_component_gallery_test "${root}/tests/cpp/QtComponentGalleryTest.cpp")
  target_link_libraries(rnqnx_component_gallery_test PRIVATE rnqnx_react_host)
  target_compile_definitions(rnqnx_component_gallery_test PRIVATE
    RNQNX_PLAYGROUND_BUNDLE_PATH="${bundle}")
  rnqnx_enable_strict_warnings(rnqnx_component_gallery_test)
  add_dependencies(rnqnx_component_gallery_test rnqnx_component_gallery_fixture)
  set_target_properties(rnqnx_component_gallery_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    target_compile_options(rnqnx_component_gallery_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(rnqnx_component_gallery_test PRIVATE -fsanitize=address)
  endif()

  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${root}/apps/component-conformance/app.json")
  file(READ "${root}/apps/component-conformance/app.json" descriptor)
  string(JSON app_id GET "${descriptor}" expo extra rnqnx appId)
  string(JSON app_name GET "${descriptor}" expo name)
  string(JSON component_name GET "${descriptor}" expo extra rnqnx componentName)
  set(lanes rnqnx_component_gallery)
  if(RNQNX_ENABLE_DESKTOP_WINDOW)
    list(APPEND lanes rnqnx_component_gallery_desktop)
  endif()
  foreach(lane IN LISTS lanes)
    set(desktop OFF)
    if(lane STREQUAL "rnqnx_component_gallery_desktop")
      set(desktop ON)
    endif()
    add_test(NAME "${lane}" COMMAND "${CMAKE_COMMAND}"
      "-DPROBE=$<TARGET_FILE:rnqnx_component_gallery_test>"
      "-DBUNDLE=${bundle}" "-DIMAGE=${image}" "-DAPP_ID=${app_id}" "-DAPP_NAME=${app_name}"
      "-DCOMPONENT_NAME=${component_name}" "-DDESKTOP=${desktop}"
      "-DARTIFACTS=${CMAKE_BINARY_DIR}/evidence/${lane}"
      -P "${root}/tests/cmake/ComponentGallery.cmake")
    set_tests_properties("${lane}" PROPERTIES TIMEOUT 180
      ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
  endforeach()
  add_test(NAME rnqnx_font_license
    COMMAND rnqnx_playground_host -platform offscreen --font-license)
  set_tests_properties(rnqnx_font_license PROPERTIES TIMEOUT 10
    PASS_REGULAR_EXPRESSION "SIL OPEN FONT LICENSE Version 1.1"
    ENVIRONMENT "QT_QUICK_BACKEND=software")
endfunction()
