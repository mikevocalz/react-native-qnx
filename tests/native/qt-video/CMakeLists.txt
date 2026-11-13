cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(RnqnxQtVideoTests LANGUAGES CXX)
  option(RNQNX_ENABLE_QT_VIDEO "Build the Qt Multimedia video component" OFF)
  include(CTest)
endif()

if(NOT RNQNX_ENABLE_QT_VIDEO)
  return()
endif()

find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick Multimedia Test)
if(NOT TARGET rnqnx_qt_video)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../../native/qt/video" "${CMAKE_CURRENT_BINARY_DIR}/video")
endif()

add_executable(rnqnx_qt_video_test VideoItemTest.cpp)
set_target_properties(rnqnx_qt_video_test PROPERTIES AUTOMOC ON)
target_compile_features(rnqnx_qt_video_test PRIVATE cxx_std_17)
target_compile_definitions(rnqnx_qt_video_test PRIVATE QT_NO_KEYWORDS
  RNQNX_QT_VIDEO_CLIP="${CMAKE_CURRENT_LIST_DIR}/clip.webm")
target_link_libraries(rnqnx_qt_video_test PRIVATE rnqnx_qt_video Qt6::Test)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_test(NAME rnqnx_qt_video_test COMMAND rnqnx_qt_video_test -platform offscreen)
  # QT_MEDIA_BACKEND picks the FFmpeg backend; the test machine has no sound
  # card, so the audio path is allowed to be absent.
  set_tests_properties(rnqnx_qt_video_test PROPERTIES TIMEOUT 60
    ENVIRONMENT "QT_QUICK_BACKEND=software;QT_MEDIA_BACKEND=ffmpeg")
endif()
