include_guard(GLOBAL)

function(rnqnx_add_nitro_qt_dispatcher)
  if(TARGET rnqnx_nitro_qt)
    return()
  endif()
  if(NOT TARGET rnqnx_react_host OR NOT DEFINED RNQNX_NITRO_CORE_ROOT)
    message(FATAL_ERROR "Qt Nitro needs the real React host and an isolated RNQNX_NITRO_CORE_ROOT source copy")
  endif()
  if(NOT CMAKE_SYSTEM_NAME MATCHES "^(QNX|Linux|Darwin)$")
    message(FATAL_ERROR "Qt Nitro dispatcher is audited only for QNX and Linux/macOS host tests")
  endif()
  file(READ "${RNQNX_NITRO_CORE_ROOT}/package.json" core_package)
  string(JSON core_version GET "${core_package}" version)
  if(NOT core_version STREQUAL "1.0.0-alpha.nightly.105.g69d365fc9b18")
    message(FATAL_ERROR "Qt Nitro dispatcher patches require the exact pinned alpha105 core")
  endif()
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
    file(REAL_PATH "${RNQNX_NITRO_CORE_ROOT}" supplied_core)
    file(REAL_PATH "${CMAKE_SOURCE_DIR}/node_modules/nitro-core" installed_core)
    if(supplied_core STREQUAL installed_core)
      message(FATAL_ERROR "Use an isolated Nitro core copy; dispatcher patches must not edit installed host dependencies")
    endif()
  endif()
  include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dependencies/NitroDispatcherPatches.cmake)
  set_target_properties(rnqnx_nitro_core PROPERTIES
    NITRO_SOURCE_COMPATIBILITY_PROFILE "rnqnx-qt-alpha105")
  add_library(rnqnx_nitro_qt STATIC
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../packages/qnx-nitro-runtime/cpp/QtNitroDispatcher.cpp")
  target_include_directories(rnqnx_nitro_qt PUBLIC
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../packages/qnx-nitro-runtime/cpp")
  target_link_libraries(rnqnx_nitro_qt PUBLIC rnqnx_nitro_core rnqnx_react_host)
  target_compile_options(rnqnx_nitro_qt PRIVATE
    "-include" "${RNQNX_NITRO_V1_COMPAT}")
  rnqnx_enable_strict_warnings(rnqnx_nitro_qt)

  add_executable(rnqnx_nitro_qt_test
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/cpp/QtNitroDispatcherTest.cpp")
  target_link_libraries(rnqnx_nitro_qt_test PRIVATE rnqnx_nitro_qt rnqnx_nitro_qnx)
  target_compile_options(rnqnx_nitro_qt_test PRIVATE
    "-include" "${RNQNX_NITRO_V1_COMPAT}")
  set(fixture_source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/fixtures/nitro-qt")
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(RNQNX_NITRO_TEST_HERMESC "" CACHE FILEPATH "Pinned host hermesc for the QNX test fixtures")
    if(NOT EXISTS "${RNQNX_NITRO_TEST_HERMESC}")
      message(FATAL_ERROR "Supply RNQNX_NITRO_TEST_HERMESC from pinned hermes-compiler 260318099.0.5")
    endif()
    set(fixture_directory "${CMAKE_BINARY_DIR}/nitro-qt-fixtures")
    file(MAKE_DIRECTORY "${fixture_directory}")
    set(fixtures "")
    foreach(name setup recover resolve reject chained void)
      set(output "${fixture_directory}/${name}.hbc")
      add_custom_command(OUTPUT "${output}"
        COMMAND "${RNQNX_NITRO_TEST_HERMESC}" -O -emit-binary -out "${output}" "${fixture_source}/${name}.js"
        DEPENDS "${fixture_source}/${name}.js" "${RNQNX_NITRO_TEST_HERMESC}"
        VERBATIM)
      list(APPEND fixtures "${output}")
    endforeach()
    add_custom_target(rnqnx_nitro_qt_fixtures DEPENDS ${fixtures})
    add_dependencies(rnqnx_nitro_qt_test rnqnx_nitro_qt_fixtures)
  else()
    set(fixture_directory "${fixture_source}")
  endif()
  target_compile_definitions(rnqnx_nitro_qt_test PRIVATE
    RNQNX_NITRO_QT_FIXTURE_DIR="${fixture_directory}")
  rnqnx_enable_strict_warnings(rnqnx_nitro_qt_test)
  set_target_properties(rnqnx_nitro_qt_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX" AND RNQNX_ENABLE_HOST_SANITIZERS)
    foreach(target rnqnx_nitro_qt rnqnx_nitro_qt_test)
      target_compile_options(${target} PRIVATE
        -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
      target_link_options(${target} PUBLIC -fsanitize=address,undefined)
    endforeach()
    set_property(SOURCE
      "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vnext/src/runtime/QtRuntimeTaskQueue.cpp"
      APPEND PROPERTY COMPILE_OPTIONS -fsanitize=address,undefined -fno-sanitize-recover=undefined)
    target_link_options(rnqnx_react_host PUBLIC -fsanitize=address,undefined)
  endif()
  if(BUILD_TESTING)
    add_test(NAME rnqnx_nitro_dispatcher_patch_contract COMMAND "${CMAKE_COMMAND}"
      "-DREPO_ROOT=${CMAKE_SOURCE_DIR}"
      "-DWORK_DIR=${CMAKE_BINARY_DIR}/nitro-dispatcher-patch-contract"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/cmake/NitroDispatcherPatchContract.cmake")
    add_test(NAME rnqnx_nitro_qt_test COMMAND rnqnx_nitro_qt_test)
    set_tests_properties(rnqnx_nitro_qt_test PROPERTIES
      TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1")
  endif()
endfunction()
