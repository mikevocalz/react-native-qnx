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

# HostWindowState (native/lifecycle): the minimize/restore channel between the
# window code and AppState. Both options add it; once per target.
function(rnqnx_add_host_window_state_sources target)
  set(dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../native/lifecycle")
  get_target_property(existing ${target} SOURCES)
  foreach(file HostWindowState.cpp HostWindowState.h)
    cmake_path(NORMAL_PATH dir OUTPUT_VARIABLE normal_dir)
    if(NOT "${normal_dir}/${file}" IN_LIST existing)
      target_sources(${target} PRIVATE "${normal_dir}/${file}")
    endif()
  endforeach()
  target_include_directories(${target} PUBLIC "${normal_dir}")
endfunction()

# Adds the chrome sources to `target` (a STATIC or executable target that
# already links Qt Quick).
function(rnqnx_add_window_chrome_sources target)
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../native/window-chrome")
  set(generated "${CMAKE_BINARY_DIR}/generated/window-chrome")
  rnqnx_generate_window_chrome_qml("${generated}")
  target_sources(${target} PRIVATE
    "${root}/WindowChromeModel.cpp" "${root}/WindowChromeModel.h"
    "${root}/WindowChrome.cpp" "${root}/WindowChrome.h"
    "${root}/WindowHost.cpp" "${root}/WindowHost.h"
    "${root}/NodeBeacon.cpp" "${root}/NodeBeacon.h"
    # Nitro windows API, GUI side (docs/nitro/API_CONTRACTS.md, S8).
    "${root}/WindowCloseGate.cpp" "${root}/WindowCloseGate.h"
    "${root}/WindowCloseRequest.h" "${root}/WindowStatePublisher.h"
    "${root}/QtWindowServices.cpp" "${root}/QtWindowServices.h")
  # WindowHost reports minimize and restore to AppState through this.
  rnqnx_add_host_window_state_sources(${target})
  # QtWindowServices implements the adapter's Qt-free service interface.
  target_include_directories(${target} PUBLIC "${root}"
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../packages/qnx-nitro-runtime/cpp"
    PRIVATE "${generated}")
  # WindowHost: single-instance restore over a local socket, and on QNX the
  # Screen calls that keep a windowed app at its own size.
  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Network)
  target_link_libraries(${target} PUBLIC Qt6::Network)
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    target_link_libraries(${target} PUBLIC screen)
  endif()
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
  # Marathon's shell owns window decorations and placement; its capability set
  # is not defined yet (docs/adr/0016).
  if(RNQNX_BUILD_MARATHON_HOST)
    message(FATAL_ERROR "RNQNX_ENABLE_WINDOW_CHROME is not supported with RNQNX_BUILD_MARATHON_HOST yet")
  endif()
  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Qml Quick)
  rnqnx_add_window_chrome_sources(rnqnx_react_host)
  target_link_libraries(rnqnx_react_host PUBLIC Qt6::Qml Qt6::Quick)
endfunction()
