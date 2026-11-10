cmake_minimum_required(VERSION 3.31.9)
project(RnqnxExpoApplicationHostTests LANGUAGES C CXX)
include(CTest)
if(CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "ExpoApplication host contract tests require a native Linux build")
endif()
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(RNQNX_REACT_NATIVE_DIR "${root}/node_modules/react-native" CACHE PATH "Pinned RN source")
set(RNQNX_NO_UPSTREAM_BUILD rnqnx-no-upstream-build)
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui)
if(NOT TARGET rnqnx_hermes)
  include("${root}/cmake/dependencies/Hermes.cmake")
endif()
if(NOT TARGET rnqnx_expo_modules_qnx)
  add_subdirectory("${root}/packages/expo-modules-core-qnx" expo-modules)
endif()
add_executable(expo_application_runtime_test RuntimeTest.cpp)
target_link_libraries(expo_application_runtime_test PRIVATE rnqnx_expo_modules_qnx rnqnx_hermes Qt6::Core Qt6::Gui)
add_test(NAME expo_application_runtime COMMAND expo_application_runtime_test)
set_tests_properties(expo_application_runtime PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(RNQNX_ENABLE_EXPO_APPLICATION)
  find_program(NODE_EXECUTABLE node REQUIRED)
  set(public_fixture "${CMAKE_CURRENT_BINARY_DIR}/official-expo-application.js")
  add_custom_command(OUTPUT "${public_fixture}"
    COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/build-public-fixture.mjs" "${public_fixture}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/build-public-fixture.mjs"
      "${root}/pnpm-lock.yaml"
      "${root}/node_modules/expo-application/package.json"
      "${root}/node_modules/expo-application/src/Application.ts"
      "${root}/node_modules/expo-application/src/ExpoApplication.ts"
      "${root}/node_modules/expo-application/src/Application.types.ts"
    VERBATIM)
  add_custom_target(expo_application_public_fixture DEPENDS "${public_fixture}")
  add_dependencies(expo_application_runtime_test expo_application_public_fixture)
  add_test(NAME expo_application_public_sdk COMMAND expo_application_runtime_test "${public_fixture}")
  set_tests_properties(expo_application_public_sdk PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
