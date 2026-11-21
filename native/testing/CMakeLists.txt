cmake_minimum_required(VERSION 3.31.9)
project(RnqnxNativeTesting LANGUAGES CXX)
# Host-only test support: a node query for Qt object trees. Nothing here is
# linked into an app, a runner or a QNX target. See docs/nitro/API_CONTRACTS.md.
option(RNQNX_BUILD_NATIVE_TESTING_TESTS "Build the native test-support library and its host tests" OFF)
if(NOT RNQNX_BUILD_NATIVE_TESTING_TESTS)
  return()
endif()
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
  message(FATAL_ERROR "The native testing lane runs on Linux or macOS hosts")
endif()
option(RNQNX_NATIVE_TESTING_SANITIZERS "Instrument the native testing lane with ASan/UBSan" OFF)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick Test)
if(RNQNX_NATIVE_TESTING_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "Native testing sanitizers require Clang or GNU")
  endif()
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(rnqnx_native_testing STATIC
  src/NodeQuery.cpp
  src/TreeScan.cpp
  src/WaitForNode.cpp)
target_compile_features(rnqnx_native_testing PUBLIC cxx_std_17)
target_include_directories(rnqnx_native_testing PUBLIC include)
target_include_directories(rnqnx_native_testing PRIVATE src)
target_link_libraries(rnqnx_native_testing PUBLIC Qt6::Core Qt6::Gui Qt6::Qml PRIVATE Qt6::Quick)
target_compile_definitions(rnqnx_native_testing PUBLIC QT_NO_KEYWORDS)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(rnqnx_native_testing PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
set(test_env "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

add_executable(rnqnx_node_query_test NodeQueryTest.cpp)
target_link_libraries(rnqnx_node_query_test PRIVATE rnqnx_native_testing Qt6::Quick Qt6::Test)
add_test(NAME rnqnx_node_query COMMAND rnqnx_node_query_test)
set_tests_properties(rnqnx_node_query PROPERTIES TIMEOUT 60 ENVIRONMENT "${test_env}")

# Smoke: a QML scene creates its node after a deferred Loader activates. The
# test waits for that node by name. The mutated case renames the node in the
# fixture; the same test must then fail, so CTest expects a failure.
add_executable(rnqnx_node_wait_smoke NodeWaitSmokeTest.cpp)
target_link_libraries(rnqnx_node_wait_smoke PRIVATE rnqnx_native_testing Qt6::Quick Qt6::Test)
target_compile_definitions(rnqnx_node_wait_smoke PRIVATE
  RNQNX_FIXTURE_QML="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/DeferredNode.qml")
add_test(NAME rnqnx_node_wait_smoke COMMAND rnqnx_node_wait_smoke)
set_tests_properties(rnqnx_node_wait_smoke PROPERTIES TIMEOUT 60 ENVIRONMENT "${test_env}")
add_test(NAME rnqnx_node_wait_smoke_mutated COMMAND rnqnx_node_wait_smoke)
set_tests_properties(rnqnx_node_wait_smoke_mutated PROPERTIES
  TIMEOUT 60 WILL_FAIL TRUE
  ENVIRONMENT "${test_env};RNQNX_FIXTURE_NODE_NAME=renamedReadyNode")
