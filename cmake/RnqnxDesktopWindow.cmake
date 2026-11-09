include_guard(GLOBAL)

option(RNQNX_ENABLE_DESKTOP_WINDOW "Enable experimental native desktop window chrome" OFF)
if(RNQNX_ENABLE_DESKTOP_WINDOW AND NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
  message(FATAL_ERROR "RNQNX_ENABLE_DESKTOP_WINDOW requires a Linux, macOS or QNX Qt host")
endif()

function(rnqnx_add_desktop_window)
  if(NOT RNQNX_ENABLE_DESKTOP_WINDOW OR TARGET rnqnx_desktop_window)
    return()
  endif()
  find_package(Qt6 6.11.1 REQUIRED COMPONENTS QuickControls2)
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  add_library(rnqnx_desktop_window STATIC "${root}/vnext/src/views/qt/QtDesktopWindow.cpp")
  target_include_directories(rnqnx_desktop_window PUBLIC "${root}/vnext/include")
  target_link_libraries(rnqnx_desktop_window PUBLIC Qt6::Quick Qt6::Qml Qt6::Gui Qt6::QuickControls2)
  target_compile_definitions(rnqnx_desktop_window PUBLIC QT_NO_KEYWORDS RNQNX_ENABLE_DESKTOP_WINDOW=1)
  rnqnx_enable_strict_warnings(rnqnx_desktop_window)
  qt_add_resources(rnqnx_desktop_window rnqnx_desktop_window
    PREFIX "/rnqnx/desktop" BASE "${root}/vnext/src/views/qt/desktop"
    FILES
      "${root}/vnext/src/views/qt/desktop/DesktopWindow.qml"
      "${root}/vnext/src/views/qt/desktop/DesktopTitleBar.qml"
      "${root}/vnext/src/views/qt/desktop/WindowButton.qml"
      "${root}/vnext/src/views/qt/desktop/WindowResizeHandle.qml")
  if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(rnqnx_desktop_window_test "${root}/tests/cpp/QtDesktopWindowTest.cpp")
    target_link_libraries(rnqnx_desktop_window_test PRIVATE rnqnx_desktop_window)
    rnqnx_enable_strict_warnings(rnqnx_desktop_window_test)
    if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
      target_compile_options(rnqnx_desktop_window_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
      target_link_options(rnqnx_desktop_window_test PRIVATE -fsanitize=address)
    endif()
    add_test(NAME rnqnx_desktop_window
      COMMAND rnqnx_desktop_window_test -platform offscreen)
    set_tests_properties(rnqnx_desktop_window PROPERTIES
      TIMEOUT 30
      ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;RNQNX_DESKTOP_TEST_SCREENSHOT=${CMAKE_CURRENT_BINARY_DIR}/desktop-window-test.png")
  endif()
endfunction()
