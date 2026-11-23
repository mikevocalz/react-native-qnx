cmake_minimum_required(VERSION 3.31.9)
project(rnqnx-timing-reader LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
option(RNQNX_ENABLE_HOST_SANITIZERS "Enable host ASan and UBSan" ON)
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
  message(FATAL_ERROR "The timing reader test runs on a Linux or Darwin host only")
endif()
set(timing_dir "${CMAKE_CURRENT_LIST_DIR}/../../../vnext/probes/timing")
add_executable(rnqnx_timing_reader_test
  TimingReaderTest.cpp
  "${timing_dir}/QnxTimingSource.cpp"
  "${timing_dir}/QnxProcessMemory.cpp")
target_include_directories(rnqnx_timing_reader_test PRIVATE "${timing_dir}")
target_compile_options(rnqnx_timing_reader_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(rnqnx_timing_reader_test PRIVATE
    -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  target_link_options(rnqnx_timing_reader_test PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME rnqnx_timing_reader_test COMMAND rnqnx_timing_reader_test)
set_tests_properties(rnqnx_timing_reader_test PROPERTIES TIMEOUT 30)
