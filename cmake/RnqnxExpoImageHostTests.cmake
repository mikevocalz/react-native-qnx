include_guard(GLOBAL)

# Host tests for the ExpoImage module and view on real Hermes. They need the
# full renderer graph (rnqnx_react_host) and the ExpoImage view
# (RNQNX_ENABLE_QT_EXPO_IMAGE).
#
#   rnqnx_expo_image_module_host_test  the module's JavaScript surface: prefetch,
#       cache functions, loadAsync, hash generators, cache read and write,
#       configureCache and the view-ref functions.
#   rnqnx_expo_image_fabric_host_test  the view through Fabric: opacity,
#       transform, onLayout, image events, ImageRef sources and view commands.
function(rnqnx_add_expo_image_host_tests)
  if(NOT RNQNX_ENABLE_QT_EXPO_IMAGE OR NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  if(NOT TARGET rnqnx_react_host)
    message(FATAL_ERROR "rnqnx_add_expo_image_host_tests requires rnqnx_react_host")
  endif()
  add_subdirectory("${RNQNX_REACT_QT_ROOT}/packages/expo-modules-core-qnx"
    "${CMAKE_BINARY_DIR}/expo-modules-core-qnx")
  add_subdirectory("${RNQNX_REACT_QT_ROOT}/packages/expo-image-qnx"
    "${CMAKE_BINARY_DIR}/expo-image-qnx")

  foreach(test IN ITEMS Module Fabric)
    string(TOLOWER "${test}" lower)
    set(name "rnqnx_expo_image_${lower}_host_test")
    add_executable(${name}
      "${RNQNX_REACT_QT_ROOT}/tests/cpp/ExpoImage${test}HostTest.cpp")
    set_target_properties(${name} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    target_sources(${name} PRIVATE
      "${RNQNX_REACT_QT_ROOT}/tests/cpp/support/ExpoImageTestSupport.h")
    target_include_directories(${name} PRIVATE "${RNQNX_REACT_QT_ROOT}/tests/cpp")
    target_link_libraries(${name} PRIVATE rnqnx_react_host)
    rnqnx_enable_strict_warnings(${name})
    if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
      target_compile_options(${name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
      target_link_options(${name} PRIVATE -fsanitize=address)
    endif()
    add_test(NAME ${name} COMMAND ${name} -platform offscreen)
    set_tests_properties(${name} PROPERTIES TIMEOUT 120
      ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1")
  endforeach()
  target_link_libraries(rnqnx_expo_image_module_host_test PRIVATE expo_image_qnx)
  target_compile_definitions(rnqnx_expo_image_module_host_test PRIVATE
    RNQNX_EXPO_POLYFILL_PATH="${RNQNX_REACT_QT_ROOT}/packages/react-native-qnx/expo/polyfill.js")
endfunction()
