cmake_minimum_required(VERSION 3.31.9)
project(RnqnxWindowChrome LANGUAGES CXX)
option(RNQNX_BUILD_WINDOW_CHROME_TESTS "Build isolated window chrome host tests" OFF)
option(RNQNX_WINDOW_CHROME_SANITIZERS "Instrument the window chrome tests with ASan/UBSan" OFF)
if(NOT RNQNX_BUILD_WINDOW_CHROME_TESTS)
  return()
endif()
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
  message(FATAL_ERROR "The isolated window chrome test lane runs on Linux or macOS hosts")
endif()
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick Test)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/RnqnxWindowChrome.cmake")

if(RNQNX_WINDOW_CHROME_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "Window chrome sanitizers require Clang or GNU")
  endif()
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(rnqnx_window_chrome STATIC)
target_compile_features(rnqnx_window_chrome PUBLIC cxx_std_17)
target_link_libraries(rnqnx_window_chrome PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
# The renderer graph builds with QT_NO_KEYWORDS (Folly defines `signals`), so the
# tests do too: plain `signals:`/`emit` would compile here and fail there.
target_compile_definitions(rnqnx_window_chrome PUBLIC QT_NO_KEYWORDS)
rnqnx_add_window_chrome_sources(rnqnx_window_chrome)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(rnqnx_window_chrome PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(rnqnx_window_chrome_model_test WindowChromeModelTest.cpp)
target_link_libraries(rnqnx_window_chrome_model_test PRIVATE rnqnx_window_chrome Qt6::Test)
add_executable(rnqnx_window_chrome_quick_test WindowChromeQuickTest.cpp)
target_link_libraries(rnqnx_window_chrome_quick_test PRIVATE rnqnx_window_chrome Qt6::Test)


include(CTest)
set(test_env "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
add_test(NAME rnqnx_window_chrome_model COMMAND rnqnx_window_chrome_model_test)
add_test(NAME rnqnx_window_chrome_quick COMMAND rnqnx_window_chrome_quick_test)
# Same chrome under a 2x device pixel ratio: logical sizes must not change and
# the rendered bar must double in device pixels.
add_test(NAME rnqnx_window_chrome_quick_dpr2 COMMAND rnqnx_window_chrome_quick_test)
# The quick tests run on a screen larger than every harness window, also at
# 2x, so a window counts as expanded only when a test makes it fill the screen.
set(quick_env "QT_QPA_PLATFORM=offscreen:configfile=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/offscreen-screen.json;QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
set_tests_properties(rnqnx_window_chrome_model PROPERTIES
  TIMEOUT 60 ENVIRONMENT "${test_env}")
set_tests_properties(rnqnx_window_chrome_quick PROPERTIES
  TIMEOUT 60 ENVIRONMENT "${quick_env}")
set_tests_properties(rnqnx_window_chrome_quick_dpr2 PROPERTIES
  TIMEOUT 60 ENVIRONMENT "${quick_env};QT_SCALE_FACTOR=2;RNQNX_EXPECT_DPR=2")

# Consumer descriptor path: CMake reads native-app.json `window`, generates the
# header main.cpp includes, and the generated values reach the C++ model.
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/RnqnxConsumerApp.cmake")
foreach(case windows-hidden-sized legacy-no-window empty-window)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${case}.json" config)
  set(dir "${CMAKE_CURRENT_BINARY_DIR}/consumer/${case}")
  file(MAKE_DIRECTORY "${dir}")
  rnqnx_consumer_window_chrome("${config}" "${dir}" header)
  string(REPLACE "-" "_" id "${case}")
  add_executable(rnqnx_window_chrome_consumer_${id} ConsumerDescriptorTest.cpp)
  target_link_libraries(rnqnx_window_chrome_consumer_${id} PRIVATE rnqnx_window_chrome)
  target_compile_definitions(rnqnx_window_chrome_consumer_${id} PRIVATE
    RNQNX_CONSUMER_WINDOW_HEADER="${header}" RNQNX_EXPECTED_CASE="${case}")
  add_test(NAME rnqnx_window_chrome_consumer_${id} COMMAND rnqnx_window_chrome_consumer_${id})
endforeach()
foreach(pair
    "bad-controls|Invalid consumer window.controls"
    "bad-style|Invalid consumer window.titleBarStyle"
    "unknown-key|window.frame is not a supported window option"
    "not-object|Consumer window must be an object"
    "size-fraction|initialSize.width must be an integer"
    "size-large|initialSize.height must be an integer"
    "size-extra|initialSize must be {width, height}"
    "size-string|initialSize.width must be an integer"
    "injection|Invalid consumer window.controls")
  string(REPLACE "|" ";" pair "${pair}")
  list(GET pair 0 case)
  list(GET pair 1 expected)
  add_test(NAME rnqnx_window_chrome_consumer_rejects_${case}
    COMMAND "${CMAKE_COMMAND}" "-DDESCRIPTOR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/invalid-${case}.json"
      "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/consumer-invalid/${case}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/ConsumerDescriptorCheck.cmake")
  set_tests_properties(rnqnx_window_chrome_consumer_rejects_${case} PROPERTIES
    WILL_FAIL OFF PASS_REGULAR_EXPRESSION "${expected}")
endforeach()
