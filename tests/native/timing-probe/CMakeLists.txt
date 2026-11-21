cmake_minimum_required(VERSION 3.31.9)
project(rnqnx-timing-probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC OFF)
include(CTest)
option(RNQNX_ENABLE_HOST_SANITIZERS "Enable host ASan and UBSan" ON)
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
  message(FATAL_ERROR "The timing probe test runs on a Linux or Darwin host only")
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
set(probe_dir "${CMAKE_CURRENT_LIST_DIR}/../../../vnext/probes/playground")
add_executable(rnqnx_timing_probe_test
  TimingProbeTest.cpp
  "${probe_dir}/TimingProbe.cpp")
target_include_directories(rnqnx_timing_probe_test PRIVATE "${probe_dir}")
target_link_libraries(rnqnx_timing_probe_test PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
target_compile_options(rnqnx_timing_probe_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(rnqnx_timing_probe_test PRIVATE
    -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  target_link_options(rnqnx_timing_probe_test PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME rnqnx_timing_probe_test COMMAND rnqnx_timing_probe_test)
set_tests_properties(rnqnx_timing_probe_test PROPERTIES
  TIMEOUT 60
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QSG_RHI_BACKEND=software")
