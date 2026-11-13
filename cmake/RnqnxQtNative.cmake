include_guard(GLOBAL)

# v2 native primitives are independently opt-in. Turning everything off keeps
# the existing QNX/Marathon Qt graph and package behavior unchanged.
option(RNQNX_ENABLE_QT_MOTION "Build experimental native Qt motion primitives" OFF)
option(RNQNX_ENABLE_QT_SVG "Build the experimental native SVG document component" OFF)
option(RNQNX_ENABLE_QT_GRAPHICS "Build the experimental Qt graphics surface observer" OFF)
option(RNQNX_ENABLE_QT_RNSVG "Build react-native-svg element rendering on Qt Quick" OFF)

# The drawer, the tab bar and the ExpoImage view are on by default in a
# consumer app build (the Pi 400 build) and off everywhere else, so the host
# renderer graphs and the Marathon target build as before unless they ask.
# Declared here as well as in RnqnxReactQt.cmake (which is included later) so
# the default below sees the same switch whatever the include order.
option(RNQNX_BUILD_CONSUMER_APP "Build an external configured consumer application" OFF)
if(RNQNX_BUILD_CONSUMER_APP)
  set(_rnqnx_native_ui_default ON)
else()
  set(_rnqnx_native_ui_default OFF)
endif()
option(RNQNX_ENABLE_QT_NATIVE_UI "Build the RNQnxDrawer and RNQnxTabBar Fabric components" ${_rnqnx_native_ui_default})
option(RNQNX_ENABLE_QT_EXPO_IMAGE "Build the ExpoImage native view (ViewManagerAdapter_ExpoImage)" ${_rnqnx_native_ui_default})
if((RNQNX_ENABLE_QT_NATIVE_UI OR RNQNX_ENABLE_QT_EXPO_IMAGE)
   AND NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
  message(FATAL_ERROR "Qt native UI components require a Linux, macOS or QNX Qt host")
endif()
if(RNQNX_ENABLE_QT_NATIVE_UI OR RNQNX_ENABLE_QT_EXPO_IMAGE)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../native/qt/nativeui"
                   "${CMAKE_BINARY_DIR}/qt-native/nativeui")
  # One test executable covers the drawer, tab bar and ExpoImage QML; it is
  # registered with the drawer and tab bar switch.
  if(RNQNX_ENABLE_QT_NATIVE_UI AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../tests/native/qt-native-ui"
                     "${CMAKE_BINARY_DIR}/qt-native/nativeui-tests")
  endif()
endif()

if((RNQNX_ENABLE_QT_MOTION OR RNQNX_ENABLE_QT_SVG OR RNQNX_ENABLE_QT_GRAPHICS OR RNQNX_ENABLE_QT_RNSVG)
   AND NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
  message(FATAL_ERROR "Qt native v2 experiments require a Linux, macOS or QNX Qt host")
endif()

foreach(feature MOTION SVG GRAPHICS RNSVG)
  if(RNQNX_ENABLE_QT_${feature})
    string(TOLOWER "${feature}" leaf)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../native/qt/${leaf}"
                     "${CMAKE_BINARY_DIR}/qt-native/${leaf}")
    if(COMMAND rnqnx_enable_strict_warnings)
      rnqnx_enable_strict_warnings(rnqnx_qt_${leaf})
    endif()
    if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
      add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../tests/native/qt-${leaf}"
                       "${CMAKE_BINARY_DIR}/qt-native/${leaf}-tests")
    endif()
  endif()
endforeach()
