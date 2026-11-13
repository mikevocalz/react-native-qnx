cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(RnqnxQtNativeUiTests LANGUAGES CXX)
  include(CTest)
endif()

# Loads the real drawer, tab bar and ExpoImage QML in an offscreen QQuickWindow
# and drives them with Qt pointer and key events. Host evidence only: no
# Fabric, no React Native, no QNX.
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick Test)
set(RNQNX_NATIVE_UI_QML_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../native/qt/nativeui")
cmake_path(NORMAL_PATH RNQNX_NATIVE_UI_QML_DIR)

add_executable(rnqnx_qt_native_ui_test NativeUiQuickTest.cpp)
set_target_properties(rnqnx_qt_native_ui_test PROPERTIES AUTOMOC ON)
target_compile_features(rnqnx_qt_native_ui_test PRIVATE cxx_std_17)
target_include_directories(rnqnx_qt_native_ui_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../vnext/include")
target_compile_definitions(rnqnx_qt_native_ui_test PRIVATE QT_NO_KEYWORDS
  RNQNX_NATIVE_UI_QML_DIR="${RNQNX_NATIVE_UI_QML_DIR}")
target_link_libraries(rnqnx_qt_native_ui_test PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(rnqnx_qt_native_ui_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_test(NAME rnqnx_qt_native_ui_test COMMAND rnqnx_qt_native_ui_test -platform offscreen)
  set_tests_properties(rnqnx_qt_native_ui_test PROPERTIES TIMEOUT 90
    ENVIRONMENT "QT_QUICK_BACKEND=software")
endif()
