cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(RnqnxQtSvg LANGUAGES CXX)
  option(RNQNX_ENABLE_QT_SVG "Build the experimental static Qt SVG document component" OFF)
  if(NOT RNQNX_ENABLE_QT_SVG)
    return()
  endif()
endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
  message(FATAL_ERROR "The Qt SVG experiment supports Linux, macOS and QNX build targets only")
endif()

find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Quick Svg)
add_library(rnqnx_qt_svg STATIC
  QtSvgItem.cpp QtSvgItem.h SvgDocument.h
  SvgDocumentValidation.cpp SvgDocumentValidation.h
  SvgPathValidation.cpp SvgPathValidation.h
  SvgTransformValidation.cpp SvgTransformValidation.h)
set_target_properties(rnqnx_qt_svg PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_features(rnqnx_qt_svg PUBLIC cxx_std_17)
target_compile_definitions(rnqnx_qt_svg PRIVATE QT_NO_KEYWORDS)
target_include_directories(rnqnx_qt_svg PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rnqnx_qt_svg PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Svg)
