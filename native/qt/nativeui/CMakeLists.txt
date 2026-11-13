cmake_minimum_required(VERSION 3.20)

# Qt Quick native UI components: the RNQnxDrawer and RNQnxTabBar Fabric
# components and the ExpoImage view. Their items are QML sources embedded as
# C++ string constants (the way the window chrome embeds WindowChrome.qml), so
# no Qt Quick runtime module beyond QtQuick itself is needed. Tint and blur on
# the ExpoImage view use QtQuick.Effects when the Qt build ships it.
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
  message(FATAL_ERROR "Qt native UI components support Linux, macOS and QNX build targets only")
endif()

foreach(qml QtDrawer QtTabBar QtExpoImage)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${qml}.qml")
    message(FATAL_ERROR "Missing ${CMAKE_CURRENT_SOURCE_DIR}/${qml}.qml")
  endif()
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${qml}.qml" qml_text)
  if(qml_text MATCHES "\\)rnqnxqml\"")
    message(FATAL_ERROR "${qml}.qml contains the raw-string delimiter")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${qml}.qml")
endforeach()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/QtDrawer.qml" RNQNX_NATIVE_UI_DRAWER_QML)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/QtTabBar.qml" RNQNX_NATIVE_UI_TAB_BAR_QML)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/QtExpoImage.qml" RNQNX_NATIVE_UI_EXPO_IMAGE_QML)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/QtNativeUiQml.h.in"
  "${generated}/QtNativeUiQml.h" @ONLY)

add_library(rnqnx_qt_nativeui INTERFACE)
target_include_directories(rnqnx_qt_nativeui INTERFACE "${generated}")
target_compile_features(rnqnx_qt_nativeui INTERFACE cxx_std_17)
