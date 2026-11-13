cmake_minimum_required(VERSION 3.31.9)
project(RnqnxMountingBenchmark LANGUAGES CXX)
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Quick Qml Gui)
get_filename_component(RNQNX_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(RNQNX_MOUNTING_SOURCE
  "${RNQNX_REPO_ROOT}/vnext/src/fabric/QtQuickMountingLayer.cpp"
  CACHE FILEPATH "Mounting implementation to benchmark; may be a saved baseline")
if(NOT EXISTS "${RNQNX_MOUNTING_SOURCE}")
  message(FATAL_ERROR "Mounting implementation not found: ${RNQNX_MOUNTING_SOURCE}")
endif()
add_executable(rnqnx_mounting_benchmark
  QtQuickMountingLayerBenchmark.cpp
  "${RNQNX_MOUNTING_SOURCE}"
  "${RNQNX_REPO_ROOT}/vnext/src/views/qt/QtQuickItemFactory.cpp"
  "${RNQNX_REPO_ROOT}/vnext/src/views/qt/QtExtraItems.cpp"
  "${RNQNX_REPO_ROOT}/vnext/src/views/qt/QnxImageProvider.cpp")
target_include_directories(rnqnx_mounting_benchmark PRIVATE
  "${RNQNX_REPO_ROOT}/vnext/include")
target_compile_definitions(rnqnx_mounting_benchmark PRIVATE QT_NO_KEYWORDS)
target_compile_features(rnqnx_mounting_benchmark PRIVATE cxx_std_20)
target_link_libraries(rnqnx_mounting_benchmark PRIVATE Qt6::Quick Qt6::Qml Qt6::Gui)
