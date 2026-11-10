include_guard(GLOBAL)

option(RNQNX_ENABLE_PLATFORM_SERVICES "Enable Qt AccessibilityInfo and BackHandler adapters" OFF)
option(RNQNX_BUILD_PLATFORM_SERVICES_TESTS "Build real RN Qt accessibility/back contract tests" OFF)

function(rnqnx_configure_platform_services)
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  target_compile_definitions(rnqnx_views_qt PUBLIC RNQNX_ENABLE_PLATFORM_SERVICES=$<BOOL:${RNQNX_ENABLE_PLATFORM_SERVICES}>)
  if(RNQNX_ENABLE_PLATFORM_SERVICES)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|QNX)$" OR NOT RNQNX_ENABLE_ACCESSIBILITY_BRIDGE)
      message(FATAL_ERROR "Qt platform services require Linux or QNX and RNQNX_ENABLE_ACCESSIBILITY_BRIDGE=ON")
    endif()
    target_sources(rnqnx_fabric_adapter PRIVATE "${root}/vnext/src/fabric/QtPlatformViewServices.cpp")
    target_sources(rnqnx_views_qt PRIVATE "${root}/vnext/src/fabric/QtPlatformViewAccessibility.cpp")
    target_sources(rnqnx_react_host PRIVATE
      "${root}/vnext/src/runtime/QtAccessibilityInfoModule.cpp"
      "${root}/vnext/src/runtime/QtDeviceEventManagerModule.cpp")
  endif()
  if(NOT RNQNX_BUILD_PLATFORM_SERVICES_TESTS)
    return()
  endif()
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Qt platform service tests require a Linux native BUILD_TESTING graph")
  endif()
  add_executable(rnqnx_platform_services_test "${root}/tests/native/platform-services/RuntimeTest.cpp")
  target_link_libraries(rnqnx_platform_services_test PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_platform_services_test)
  set_target_properties(rnqnx_platform_services_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rnqnx_platform_services_test PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(rnqnx_platform_services_test PRIVATE -fsanitize=address,undefined)
  endif()
  if(RNQNX_ENABLE_PLATFORM_SERVICES)
    add_executable(rnqnx_platform_accessible_view_test "${root}/tests/native/platform-services/AccessibleViewTest.cpp")
    target_link_libraries(rnqnx_platform_accessible_view_test PRIVATE rnqnx_views_qt)
    rnqnx_enable_strict_warnings(rnqnx_platform_accessible_view_test)
    add_test(NAME rnqnx_platform_accessible_view COMMAND rnqnx_platform_accessible_view_test)
    set_tests_properties(rnqnx_platform_accessible_view PROPERTIES TIMEOUT 15
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
  endif()
  foreach(platform qnx marathon)
    set(config "metro.public.config.js")
    if(platform STREQUAL "marathon")
      set(config "metro.public.marathon.config.js")
    endif()
    set(bundle "${CMAKE_BINARY_DIR}/rnqnx-platform-services-${platform}.bundle.js")
    add_custom_command(OUTPUT "${bundle}"
      COMMAND "${root}/node_modules/.bin/metro" build platform-services-probe.js --config "${config}"
        --platform "${platform}" --dev false --max-workers 2 --out "${bundle}"
      WORKING_DIRECTORY "${root}/apps/rn-playground"
      DEPENDS "${root}/apps/rn-playground/platform-services-probe.js"
        "${root}/apps/rn-playground/${config}"
        "${root}/packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js"
        "${root}/packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js"
        "${root}/packages/react-native-qnx/Libraries/Components/AccessibilityInfo/AccessibilityInfo.js"
        "${root}/packages/react-native-qnx/Libraries/Utilities/BackHandler.js"
        "${root}/pnpm-lock.yaml"
      VERBATIM)
    add_custom_target(rnqnx_platform_services_${platform}_bundle DEPENDS "${bundle}")
    add_dependencies(rnqnx_platform_services_test rnqnx_platform_services_${platform}_bundle)
    add_test(NAME rnqnx_platform_services_${platform} COMMAND rnqnx_platform_services_test "${bundle}" "${platform}")
    set_tests_properties(rnqnx_platform_services_${platform} PROPERTIES TIMEOUT 30
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
  endforeach()
endfunction()
