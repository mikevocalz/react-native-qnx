include_guard(GLOBAL)

# react-native-gesture-handler 3 on Qt. Default OFF; needs the Worklets target.
#
# The recognizer registry (vnext/src/gesture/GestureHandlerCore.cpp) is ours and
# has no Qt or React dependency. The library's own C++ for the detector shadow
# node and the runtime decorator is compiled from the installed package, with
# the Fabric codegen output generated into the build directory.

option(RNQNX_ENABLE_GESTURE_HANDLER "Build react-native-gesture-handler 3 on the Qt Worklets platform" OFF)
set(RNQNX_GESTURE_HANDLER_SOURCE_DIR "" CACHE PATH "Installed react-native-gesture-handler package root")

function(rnqnx_add_gesture_handler_target)
  if(TARGET rnqnx_gesture_handler)
    return()
  endif()
  if(NOT TARGET rnqnx_worklets)
    message(FATAL_ERROR "RNQNX_ENABLE_GESTURE_HANDLER needs RNQNX_ENABLE_WORKLETS")
  endif()
  if(NOT RNQNX_GESTURE_HANDLER_SOURCE_DIR)
    set(RNQNX_GESTURE_HANDLER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/node_modules/react-native-gesture-handler")
  endif()
  get_filename_component(pkg "${RNQNX_GESTURE_HANDLER_SOURCE_DIR}" REALPATH)
  if(NOT EXISTS "${pkg}/package.json" OR NOT IS_DIRECTORY "${pkg}/shared/shadowNodes")
    message(FATAL_ERROR "RNQNX_GESTURE_HANDLER_SOURCE_DIR must be an installed react-native-gesture-handler package: ${pkg}")
  endif()
  file(READ "${pkg}/package.json" pkg_json)
  string(JSON pkg_name GET "${pkg_json}" name)
  string(JSON pkg_version GET "${pkg_json}" version)
  if(NOT pkg_name STREQUAL "react-native-gesture-handler" OR NOT pkg_version MATCHES "^3\\.[3-5]\\.[0-9]+$")
    message(FATAL_ERROR "react-native-gesture-handler 3.3 to 3.5 is required (found ${pkg_name} ${pkg_version})")
  endif()

  find_program(RNQNX_NODE node REQUIRED)
  set(gen "${CMAKE_BINARY_DIR}/generated/gesture-handler/fabric")
  execute_process(
    COMMAND "${RNQNX_NODE}" "${RNQNX_REACT_QT_ROOT}/scripts/fabric-codegen.mjs" rngesturehandler_codegen "${gen}"
      "${pkg}/src/specs/RNGestureHandlerButtonNativeComponent.ts"
      "${pkg}/src/specs/RNGestureHandlerDetectorNativeComponent.ts"
      "${pkg}/src/specs/RNGestureHandlerRootViewNativeComponent.ts"
    WORKING_DIRECTORY "${RNQNX_REACT_QT_ROOT}"
    RESULT_VARIABLE codegen_result ERROR_VARIABLE codegen_error)
  if(NOT codegen_result EQUAL 0)
    message(FATAL_ERROR "Gesture Handler Fabric codegen failed: ${codegen_error}")
  endif()
  file(GLOB generated_sources "${gen}/react/renderer/components/rngesturehandler_codegen/*.cpp")
  set(shadow "${pkg}/shared/shadowNodes/react/renderer/components/rngesturehandler_codegen")
  # The package's descriptors header replaces the generated one: it adds the
  # detector's custom descriptor. Both define the same include path, so the
  # package directory goes first and the generated copy of ComponentDescriptors
  # is not compiled.
  list(FILTER generated_sources EXCLUDE REGEX "/ComponentDescriptors\\.cpp$")

  set(platform_dir "${RNQNX_REACT_QT_ROOT}/vnext/src/gesture")
  add_library(rnqnx_gesture_handler STATIC
    "${RNQNX_REACT_QT_ROOT}/vnext/src/gesture/GestureHandlerCore.cpp"
    "${platform_dir}/QnxGestureHandlerModule.cpp"
    "${platform_dir}/QtGestureInput.cpp"
    "${platform_dir}/QtGesturePlatform.cpp"
    "${shadow}/RNGestureHandlerDetectorShadowNode.cpp"
    "${pkg}/shared/runtime/RNGHRuntimeDecorator.cpp"
    ${generated_sources})
  set_source_files_properties(
    "${shadow}/RNGestureHandlerDetectorShadowNode.cpp" "${pkg}/shared/runtime/RNGHRuntimeDecorator.cpp" ${generated_sources}
    PROPERTIES COMPILE_OPTIONS "-w")
  target_include_directories(rnqnx_gesture_handler
    PUBLIC "${RNQNX_REACT_QT_ROOT}/vnext/include"
    PRIVATE "${shadow}" "${pkg}/shared" "${pkg}/shared/shadowNodes" "${gen}" "${platform_dir}")
  target_compile_definitions(rnqnx_gesture_handler PUBLIC RNQNX_ENABLE_GESTURE_HANDLER=1 PRIVATE RNGH_USE_WORKLETS=1)
  target_link_libraries(rnqnx_gesture_handler PUBLIC rnqnx_worklets rnqnx_react_host Qt6::Quick)
  rnqnx_enable_strict_warnings(rnqnx_gesture_handler)
  # Generated Fabric headers use #pragma mark.
  target_compile_options(rnqnx_gesture_handler PRIVATE -Wno-unknown-pragmas)
  message(STATUS "react-native-gesture-handler ${pkg_version} from ${pkg}")
endfunction()
