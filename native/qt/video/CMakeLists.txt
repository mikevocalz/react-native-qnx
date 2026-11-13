cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(RnqnxQtVideo LANGUAGES CXX)
  option(RNQNX_ENABLE_QT_VIDEO "Build the Qt Multimedia video component" OFF)
  if(NOT RNQNX_ENABLE_QT_VIDEO)
    return()
  endif()
endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
  message(FATAL_ERROR "The Qt video component supports Linux, macOS and QNX build targets only")
endif()

find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick Multimedia)
add_library(rnqnx_qt_video STATIC QtVideoItem.cpp QtVideoItem.h)
set_target_properties(rnqnx_qt_video PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_features(rnqnx_qt_video PUBLIC cxx_std_17)
target_compile_definitions(rnqnx_qt_video PRIVATE QT_NO_KEYWORDS)
target_include_directories(rnqnx_qt_video PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rnqnx_qt_video PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia)

# The QML is compiled into the binary. A static library's resources are
# dropped by the linker unless they are an object library or initialised by
# name, so use the OBJECT form and let qt_add_resources attach them.
qt_add_resources(rnqnx_qt_video rnqnx_qt_video_qml
  PREFIX "/react-native-qnx/video"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}"
  FILES QnxVideo.qml)
