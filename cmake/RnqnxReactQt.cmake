include_guard(GLOBAL)

get_filename_component(RNQNX_REACT_QT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

include(${CMAKE_CURRENT_LIST_DIR}/RnqnxWebSockets.cmake)

function(rnqnx_add_qt_views_target)
  if(TARGET rnqnx_views_qt)
    return()
  endif()

  add_library(rnqnx_views_qt STATIC
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/QtQuickItemFactory.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/QnxImageProvider.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtQuickMountingLayer.cpp"
  )
  target_sources(rnqnx_views_qt PRIVATE "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/QtTextInputQml.cpp")
  target_compile_definitions(rnqnx_views_qt PUBLIC RNQNX_ENABLE_TEXT_INPUT_CONTRACT=$<BOOL:${RNQNX_ENABLE_TEXT_INPUT_CONTRACT}>)
  if(RNQNX_ENABLE_TEXT_INPUT_CONTRACT)
    target_sources(rnqnx_views_qt PRIVATE
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtTextInputBridge.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtTextInputBridge.h")
    set_target_properties(rnqnx_views_qt PROPERTIES AUTOMOC ON)
  endif()
  target_include_directories(rnqnx_views_qt PUBLIC
    "${RNQNX_REACT_QT_ROOT}/vnext/include"
  )
  target_link_libraries(rnqnx_views_qt PUBLIC Qt6::Quick Qt6::Qml Qt6::Gui)

  # Qt's keyword macros collide with React Native's EventEmitter::emit.
  target_compile_definitions(rnqnx_views_qt PUBLIC QT_NO_KEYWORDS)
  rnqnx_enable_strict_warnings(rnqnx_views_qt)
endfunction()

function(rnqnx_add_fabric_adapter_target)
  if(TARGET rnqnx_fabric_adapter)
    return()
  endif()
  if(NOT TARGET rnqnx_react_native_renderer)
    message(FATAL_ERROR
      "rnqnx_add_fabric_adapter_target requires rnqnx_react_native_renderer")
  endif()

  rnqnx_add_qt_views_target()

  add_library(rnqnx_fabric_adapter STATIC
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/ShadowViewMutationAdapter.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtTouchDispatcher.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtScrollBridge.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtScrollBridge.h"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtSwitchBridge.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtSwitchBridge.h"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtImageBridge.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtImageBridge.h"
  )
  target_link_libraries(rnqnx_fabric_adapter PUBLIC
    rnqnx_react_native_renderer
    rnqnx_views_qt
  )
  set_target_properties(rnqnx_fabric_adapter PROPERTIES AUTOMOC ON)
  target_compile_definitions(rnqnx_fabric_adapter PUBLIC
    RNQNX_ENABLE_INPUT_LIFECYCLE=$<BOOL:${RNQNX_ENABLE_INPUT_LIFECYCLE}>)
  rnqnx_enable_strict_warnings(rnqnx_fabric_adapter)
endfunction()

function(rnqnx_add_react_host_target)
  if(TARGET rnqnx_react_host)
    return()
  endif()
  if(NOT TARGET rnqnx_hermes)
    message(FATAL_ERROR "rnqnx_add_react_host_target requires rnqnx_hermes")
  endif()

  rnqnx_add_fabric_adapter_target()

  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Network)

  add_library(rnqnx_react_host STATIC
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QtReactHost.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QtRuntimeTaskQueue.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxExceptionsManager.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxFileReaderModule.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxImageLoader.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QtImageCache.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxSegmentFetcher.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxWebSocketModule.cpp"
  )
  target_link_libraries(rnqnx_react_host PUBLIC
    rnqnx_fabric_adapter
    rnqnx_react_native_renderer
    rnqnx_hermes
    Qt6::Network
  )
  if(RNQNX_ENABLE_FRAME_TRACE)
    target_link_libraries(rnqnx_react_host PUBLIC rnqnx_runtime_trace)
  endif()
  if(RNQNX_ENABLE_WEBSOCKETS)
    target_compile_definitions(rnqnx_react_host PRIVATE RNQNX_ENABLE_WEBSOCKETS)
    target_link_libraries(rnqnx_react_host PUBLIC rnqnx_websocket_transport)
  endif()
  rnqnx_enable_strict_warnings(rnqnx_react_host)
  if(RNQNX_ENABLE_TEXT_INPUT_CONTRACT)
    include(${RNQNX_REACT_QT_ROOT}/cmake/RnqnxTextInput.cmake)
    rnqnx_add_text_input_probe()
  endif()
endfunction()
