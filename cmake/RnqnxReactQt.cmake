include_guard(GLOBAL)

get_filename_component(RNQNX_REACT_QT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

include(${CMAKE_CURRENT_LIST_DIR}/RnqnxWebSockets.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/RnqnxDevSupport.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/RnqnxAppState.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/RnqnxWindowChrome.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/RnqnxWorklets.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/RnqnxReanimated.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/RnqnxGestureHandler.cmake)

# Add the opt-in consumer only after the existing runtime and optional native
# adapters have been declared. The OFF path leaves all original targets alone.
option(RNQNX_BUILD_CONSUMER_APP "Build an external configured consumer application" OFF)
option(RNQNX_CONSUMER_HOST_PREVIEW "Explicitly preview a QNX consumer on Linux (not QNX proof)" OFF)
set(RNQNX_CONSUMER_CONFIG "" CACHE FILEPATH "Generated consumer native-app.json")
if(RNQNX_BUILD_CONSUMER_APP)
  include(${CMAKE_CURRENT_LIST_DIR}/RnqnxConsumerApp.cmake)
  rnqnx_consumer_add_module_packages()
  cmake_language(DEFER CALL rnqnx_add_consumer_app)
endif()

function(rnqnx_add_qt_views_target)
  if(TARGET rnqnx_views_qt)
    return()
  endif()

  add_library(rnqnx_views_qt STATIC
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/QtQuickItemFactory.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/QtExtraItems.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/QtModalQml.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/QnxImageProvider.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ImageDiskCache.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/QtImageFetcher.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ImageFetchQueue.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ImageRefRegistry.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ImageLoadedNotifier.cpp"
    # The image loader module decodes, encodes and holds ImageRefs for every
    # app, not only those that mount the ExpoImage view.
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ImageDecode.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/BlurHash.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ThumbHash.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/BlurHashEncoder.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ThumbHashEncoder.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ImageEffects.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtQuickMountingLayer.cpp"
  )
  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Network)
  target_link_libraries(rnqnx_views_qt PUBLIC Qt6::Network)
  target_sources(rnqnx_views_qt PRIVATE "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/QtTextInputQml.cpp")
  target_compile_definitions(rnqnx_views_qt PUBLIC RNQNX_ENABLE_TEXT_INPUT_CONTRACT=$<BOOL:${RNQNX_ENABLE_TEXT_INPUT_CONTRACT}>)
  if(RNQNX_ENABLE_TEXT_INPUT_CONTRACT)
    target_sources(rnqnx_views_qt PRIVATE
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtTextInputBridge.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtTextInputBridge.h")
    set_target_properties(rnqnx_views_qt PROPERTIES AUTOMOC ON)
  endif()
  target_compile_definitions(rnqnx_views_qt PUBLIC RNQNX_ENABLE_ACCESSIBILITY_BRIDGE=$<BOOL:${RNQNX_ENABLE_ACCESSIBILITY_BRIDGE}>)
  if(RNQNX_ENABLE_ACCESSIBILITY_BRIDGE)
    if(NOT QT_FEATURE_accessibility)
      message(FATAL_ERROR "Accessibility bridge requires Qt accessibility support")
    endif()
    target_sources(rnqnx_views_qt PRIVATE "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtViewAccessibility.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtAccessibilityActivation.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtAccessibilityActivation.h")
    set_target_properties(rnqnx_views_qt PROPERTIES AUTOMOC ON)
  endif()
  target_include_directories(rnqnx_views_qt PUBLIC
    "${RNQNX_REACT_QT_ROOT}/vnext/include"
  )
  target_link_libraries(rnqnx_views_qt PUBLIC Qt6::Quick Qt6::Qml Qt6::Gui)
  target_compile_definitions(rnqnx_views_qt PUBLIC
    RNQNX_ENABLE_QT_SVG=$<BOOL:${RNQNX_ENABLE_QT_SVG}>
    RNQNX_ENABLE_QT_VIDEO=$<BOOL:${RNQNX_ENABLE_QT_VIDEO}>)
  if(RNQNX_ENABLE_QT_VIDEO)
    target_link_libraries(rnqnx_views_qt PUBLIC rnqnx_qt_video)
  endif()
  if(RNQNX_ENABLE_QT_SVG)
    target_link_libraries(rnqnx_views_qt PUBLIC rnqnx_qt_svg)
  endif()
  target_compile_definitions(rnqnx_views_qt PUBLIC
    RNQNX_ENABLE_QT_RNSVG=$<BOOL:${RNQNX_ENABLE_QT_RNSVG}>)
  if(RNQNX_ENABLE_QT_RNSVG)
    target_link_libraries(rnqnx_views_qt PUBLIC rnqnx_qt_rnsvg)
  endif()
  target_compile_definitions(rnqnx_views_qt PUBLIC
    RNQNX_ENABLE_QT_NATIVE_UI=$<BOOL:${RNQNX_ENABLE_QT_NATIVE_UI}>
    RNQNX_ENABLE_QT_EXPO_IMAGE=$<BOOL:${RNQNX_ENABLE_QT_EXPO_IMAGE}>)
  if(RNQNX_ENABLE_QT_NATIVE_UI OR RNQNX_ENABLE_QT_EXPO_IMAGE)
    target_link_libraries(rnqnx_views_qt PUBLIC rnqnx_qt_nativeui)
  endif()
  if(RNQNX_ENABLE_QT_EXPO_IMAGE)
    # The painted ExpoImage view: it draws with QPainter, so it needs no Qt
    # module beyond Quick. The header is listed so AUTOMOC runs on it.
    target_sources(rnqnx_views_qt PRIVATE
      "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/views/image/ExpoImageItem.h"
      "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ExpoImageItem.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/src/views/qt/image/ImageLayout.cpp")
    set_target_properties(rnqnx_views_qt PROPERTIES AUTOMOC ON)
  endif()

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

  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Network)

  add_library(rnqnx_fabric_adapter STATIC
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/ShadowViewMutationAdapter.cpp"
    # QtRnsvgImageProvider (globbed below) constructs a QtImageCache, so the
    # cache is defined here, once, and rnqnx_react_host reaches it through
    # this target. Host tests that link only the adapter then resolve it too.
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QtImageCache.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtTouchDispatcher.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtScrollBridge.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtScrollBridge.h"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtModalBridge.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtModalBridge.h"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtSwitchBridge.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtSwitchBridge.h"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtImageBridge.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtImageBridge.h"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtExtraComponents.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtExtraComponents.h"
  )
  target_link_libraries(rnqnx_fabric_adapter PUBLIC
    rnqnx_react_native_renderer
    rnqnx_views_qt
    Qt6::Network
  )
  set_target_properties(rnqnx_fabric_adapter PROPERTIES AUTOMOC ON)
  if(RNQNX_ENABLE_QT_VIDEO)
    set(video_generated "${RNQNX_REACT_QT_ROOT}/vnext/generated/react/renderer/components/RnqnxQtVideo")
    target_sources(rnqnx_fabric_adapter PRIVATE
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtVideoBridge.cpp"
      "${video_generated}/ComponentDescriptors.cpp"
      "${video_generated}/EventEmitters.cpp"
      "${video_generated}/Props.cpp"
      "${video_generated}/ShadowNodes.cpp"
      "${video_generated}/States.cpp")
    target_include_directories(rnqnx_fabric_adapter SYSTEM PUBLIC
      "${RNQNX_REACT_QT_ROOT}/vnext/generated")
    set_source_files_properties(
      "${video_generated}/ComponentDescriptors.cpp" "${video_generated}/EventEmitters.cpp"
      "${video_generated}/Props.cpp" "${video_generated}/ShadowNodes.cpp"
      "${video_generated}/States.cpp" PROPERTIES COMPILE_OPTIONS
      "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-unused-parameter;-Wno-unknown-pragmas>")
  endif()
  if(RNQNX_ENABLE_QT_SVG)
    set(svg_generated "${RNQNX_REACT_QT_ROOT}/vnext/generated/react/renderer/components/RnqnxQtSvg")
    target_sources(rnqnx_fabric_adapter PRIVATE
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtSvgBridge.cpp"
      "${svg_generated}/ComponentDescriptors.cpp"
      "${svg_generated}/EventEmitters.cpp"
      "${svg_generated}/Props.cpp"
      "${svg_generated}/ShadowNodes.cpp"
      "${svg_generated}/States.cpp")
    # Upstream generated headers contain warning patterns outside our control.
    target_include_directories(rnqnx_fabric_adapter SYSTEM PUBLIC
      "${RNQNX_REACT_QT_ROOT}/vnext/generated")
    set_source_files_properties(
      "${svg_generated}/ComponentDescriptors.cpp" "${svg_generated}/EventEmitters.cpp"
      "${svg_generated}/Props.cpp" "${svg_generated}/ShadowNodes.cpp"
      "${svg_generated}/States.cpp" PROPERTIES COMPILE_OPTIONS
      "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-unused-parameter;-Wno-unknown-pragmas>")
  endif()
  if(RNQNX_ENABLE_QT_RNSVG)
    # Props and events are generated from the pinned react-native-svg specs
    # (pnpm qt:rnsvg:check); shadow nodes and layout are that package's own C++.
    set(rnsvg_package "${RNQNX_REACT_QT_ROOT}/node_modules/react-native-svg")
    set(rnsvg_cpp "${rnsvg_package}/common/cpp/react/renderer/components/rnsvg")
    if(NOT EXISTS "${rnsvg_cpp}/RNSVGLayoutableShadowNode.cpp")
      message(FATAL_ERROR "RNQNX_ENABLE_QT_RNSVG needs react-native-svg 15.16.0 installed (pnpm install): ${rnsvg_cpp}")
    endif()
    set(rnsvg_generated "${RNQNX_REACT_QT_ROOT}/vnext/generated/react/renderer/components/rnsvg")
    # The bridge is one file per kind of work (values, shapes, defs, text,
    # shadow nodes, layout events, filters, images); the list is a glob so a
    # new one needs no edit here.
    file(GLOB rnsvg_bridge_sources CONFIGURE_DEPENDS
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtRnsvg*.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtRnsvg*.h"
      "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtRnsvg*.h")
    target_sources(rnqnx_fabric_adapter PRIVATE
      ${rnsvg_bridge_sources}
      "${rnsvg_cpp}/RNSVGLayoutableShadowNode.cpp"
      "${rnsvg_generated}/EventEmitters.cpp"
      "${rnsvg_generated}/Props.cpp")
    target_include_directories(rnqnx_fabric_adapter SYSTEM PUBLIC
      "${RNQNX_REACT_QT_ROOT}/vnext/generated" "${rnsvg_cpp}")
    # Only the three third-party translation units are relaxed: the react-native-svg
    # package's RNSVGLayoutableShadowNode.cpp and the codegen output of its specs
    # (unused parameters, unknown pragmas, deprecated React Native calls, signed
    # comparisons). The QtRnsvg* bridge sources are ours and build with the full
    # strict warning set.
    set_source_files_properties(
      "${rnsvg_cpp}/RNSVGLayoutableShadowNode.cpp"
      "${rnsvg_generated}/EventEmitters.cpp" "${rnsvg_generated}/Props.cpp"
      PROPERTIES COMPILE_OPTIONS
      "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-unused-parameter;-Wno-unknown-pragmas;-Wno-deprecated-declarations;-Wno-sign-compare>")
  endif()
  if(RNQNX_ENABLE_QT_NATIVE_UI OR RNQNX_ENABLE_QT_EXPO_IMAGE)
    # Props, events, shadow nodes and descriptors are generated from the specs
    # in packages/react-native-qnx/qt (pnpm qt:native-ui:check). QtNativeUiBridge
    # turns them into Qt Quick item properties and routes the item's events back.
    set(native_ui_generated_root "${RNQNX_REACT_QT_ROOT}/vnext/generated/react/renderer/components")
    set(native_ui_libraries "")
    if(RNQNX_ENABLE_QT_NATIVE_UI)
      list(APPEND native_ui_libraries RnqnxQtDrawer RnqnxQtTabBar)
    endif()
    if(RNQNX_ENABLE_QT_EXPO_IMAGE)
      list(APPEND native_ui_libraries RnqnxExpoImage)
    endif()
    target_sources(rnqnx_fabric_adapter PRIVATE
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtNativeUiBridge.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/fabric/QtNativeUiBridge.h")
    if(RNQNX_ENABLE_QT_EXPO_IMAGE)
      target_sources(rnqnx_fabric_adapter PRIVATE
        "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtExpoImageBridge.cpp")
    endif()
    foreach(library IN LISTS native_ui_libraries)
      set(native_ui_generated "${native_ui_generated_root}/${library}")
      set(native_ui_sources
        "${native_ui_generated}/ComponentDescriptors.cpp"
        "${native_ui_generated}/EventEmitters.cpp"
        "${native_ui_generated}/Props.cpp"
        "${native_ui_generated}/ShadowNodes.cpp"
        "${native_ui_generated}/States.cpp")
      target_sources(rnqnx_fabric_adapter PRIVATE ${native_ui_sources})
      # Upstream generated sources contain warning patterns outside our control.
      set_source_files_properties(${native_ui_sources} PROPERTIES COMPILE_OPTIONS
        "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-unused-parameter;-Wno-unknown-pragmas>")
    endforeach()
    target_include_directories(rnqnx_fabric_adapter SYSTEM PUBLIC
      "${RNQNX_REACT_QT_ROOT}/vnext/generated")
    set_source_files_properties("${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtNativeUiBridge.cpp"
      "${RNQNX_REACT_QT_ROOT}/vnext/src/fabric/QtExpoImageBridge.cpp"
      PROPERTIES COMPILE_OPTIONS
      "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-unused-parameter;-Wno-unknown-pragmas>")
  endif()
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
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxImageLoaderRefs.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QtImageCache.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxSegmentFetcher.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxWebSocketModule.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxDevSupportModules.cpp"
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
  rnqnx_configure_app_state()
  rnqnx_configure_window_chrome()
  rnqnx_configure_dev_support()
  include(${RNQNX_REACT_QT_ROOT}/cmake/RnqnxQtSvgFabric.cmake)
  rnqnx_add_qt_svg_fabric_test()
  include(${RNQNX_REACT_QT_ROOT}/cmake/RnqnxQtRnsvgFabric.cmake)
  rnqnx_add_qt_rnsvg_fabric_test()
  rnqnx_add_qt_rnsvg_image_fabric_test()
  rnqnx_add_qt_rnsvg_corpus_test()
  rnqnx_add_qt_rnsvg_values_test()
  rnqnx_enable_strict_warnings(rnqnx_react_host)
  if(RNQNX_ENABLE_WORKLETS)
    rnqnx_add_worklets_target()
  endif()
  if(RNQNX_ENABLE_REANIMATED)
    rnqnx_add_reanimated_target()
  endif()
  if(RNQNX_ENABLE_GESTURE_HANDLER)
    rnqnx_add_gesture_handler_target()
  endif()
  if(RNQNX_ENABLE_ACCESSIBILITY_BRIDGE)
    include(${RNQNX_REACT_QT_ROOT}/cmake/RnqnxAccessibilityActions.cmake)
    rnqnx_add_accessibility_action_probe()
    add_executable(rnqnx_accessibility_view_test "${RNQNX_REACT_QT_ROOT}/tests/native/accessibility/ViewTest.cpp")
    target_link_libraries(rnqnx_accessibility_view_test PRIVATE rnqnx_views_qt)
    target_compile_definitions(rnqnx_accessibility_view_test PRIVATE RNQNX_ACCESSIBILITY_MOUNTING_TEST=1)
    rnqnx_enable_strict_warnings(rnqnx_accessibility_view_test)
    if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
      add_test(NAME rnqnx_accessibility_view_test COMMAND rnqnx_accessibility_view_test -platform offscreen)
      set_tests_properties(rnqnx_accessibility_view_test PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
    endif()
  endif()
  if(RNQNX_ENABLE_TEXT_INPUT_CONTRACT)
    include(${RNQNX_REACT_QT_ROOT}/cmake/RnqnxTextInput.cmake)
    rnqnx_add_text_input_probe()
  endif()
endfunction()
