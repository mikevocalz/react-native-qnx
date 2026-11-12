include_guard(GLOBAL)

# Client-drawn title bar and window controls (docs/adr/0016). Default OFF: with
# it off, the playground and consumer hosts build exactly as before.
option(RNQNX_ENABLE_WINDOW_CHROME "Draw the Qt Quick title bar and window controls" OFF)

# Generates WindowChromeQml.generated.h from WindowChrome.qml into `out_dir`.
function(rnqnx_generate_window_chrome_qml out_dir)
  set(source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../native/window-chrome/WindowChrome.qml")
  file(READ "${source}" RNQNX_WINDOW_CHROME_QML)
  if(RNQNX_WINDOW_CHROME_QML MATCHES "\\)rnqnxqml\"")
    message(FATAL_ERROR "WindowChrome.qml contains the raw-string delimiter")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
  configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/WindowChromeQml.h.in"
    "${out_dir}/WindowChromeQml.generated.h" @ONLY)
endfunction()

# Adds the chrome sources to `target` (a STATIC or executable target that
# already links Qt Quick).
function(rnqnx_add_window_chrome_sources target)
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../native/window-chrome")
  set(generated "${CMAKE_BINARY_DIR}/generated/window-chrome")
  rnqnx_generate_window_chrome_qml("${generated}")
  target_sources(${target} PRIVATE
    "${root}/WindowChromeModel.cpp" "${root}/WindowChromeModel.h"
    "${root}/WindowChrome.cpp" "${root}/WindowChrome.h")
  target_include_directories(${target} PUBLIC "${root}" PRIVATE "${generated}")
  set_target_properties(${target} PROPERTIES AUTOMOC ON)
endfunction()

function(rnqnx_configure_window_chrome)
  target_compile_definitions(rnqnx_react_host PUBLIC
    RNQNX_ENABLE_WINDOW_CHROME=$<BOOL:${RNQNX_ENABLE_WINDOW_CHROME}>)
  if(NOT RNQNX_ENABLE_WINDOW_CHROME)
    return()
  endif()
  if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
    message(FATAL_ERROR "RNQNX_ENABLE_WINDOW_CHROME supports Linux, macOS and QNX Qt hosts")
  endif()
  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Qml Quick)
  rnqnx_add_window_chrome_sources(rnqnx_react_host)
  target_link_libraries(rnqnx_react_host PUBLIC Qt6::Qml Qt6::Quick)
endfunction()
