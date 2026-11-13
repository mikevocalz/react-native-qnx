cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(RnqnxQtRnsvg LANGUAGES CXX)
  option(RNQNX_ENABLE_QT_RNSVG "Build react-native-svg element rendering" OFF)
  if(NOT RNQNX_ENABLE_QT_RNSVG)
    return()
  endif()
endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
  message(FATAL_ERROR "react-native-svg rendering supports Linux, macOS and QNX build targets only")
endif()

# Core, Gui and Quick only: this renderer paints with QPainter and does not
# link Qt6::Svg, so it adds no module the QNX Qt runtime would have to carry.
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Quick)

# One directory per responsibility: the model and Qt items at this level,
# then scene/ (traversal and painting), text/, filter/ and image/. Sources are
# collected so a new file in one of them needs no edit to this list.
file(GLOB RNQNX_RNSVG_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/scene/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/scene/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/text/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/text/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/filter/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/filter/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/image/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/image/*.h")
add_library(rnqnx_qt_rnsvg STATIC ${RNQNX_RNSVG_SOURCES})
set_target_properties(rnqnx_qt_rnsvg PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_features(rnqnx_qt_rnsvg PUBLIC cxx_std_17)
target_compile_definitions(rnqnx_qt_rnsvg PRIVATE QT_NO_KEYWORDS)
target_include_directories(rnqnx_qt_rnsvg PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rnqnx_qt_rnsvg PUBLIC Qt6::Core Qt6::Gui Qt6::Quick)
