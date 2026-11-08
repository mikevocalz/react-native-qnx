include_guard(GLOBAL)
option(RNQNX_ENABLE_APP_STATE "Wire Qt lifecycle to React Native AppState" OFF)
option(RNQNX_BUILD_APPSTATE_TESTS "Build real Hermes AppState contract tests" OFF)
option(RNQNX_APPSTATE_SANITIZERS "Instrument AppState adapter/provider and test (not prebuilt Qt/RN/Hermes)" OFF)

function(rnqnx_configure_app_state)
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  target_compile_definitions(rnqnx_react_host PUBLIC RNQNX_ENABLE_APP_STATE=$<BOOL:${RNQNX_ENABLE_APP_STATE}>)
  if(RNQNX_ENABLE_APP_STATE)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|QNX)$")
      message(FATAL_ERROR "Experimental AppState supports QNX or Linux native hosts only")
    endif()
    target_sources(rnqnx_react_host PRIVATE
      "${root}/vnext/src/runtime/QnxAppStateModule.cpp"
      "${root}/native/lifecycle/QtApplicationLifecycle.cpp"
      "${root}/native/lifecycle/QtApplicationLifecycle.h")
    target_include_directories(rnqnx_react_host PRIVATE "${root}/native/lifecycle")
    set_target_properties(rnqnx_react_host PROPERTIES AUTOMOC ON)
  endif()
  if(RNQNX_APPSTATE_SANITIZERS)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      message(FATAL_ERROR "AppState sanitizer evidence is Linux Clang/GNU only")
    endif()
    if(RNQNX_ENABLE_APP_STATE)
      set_source_files_properties(
        "${root}/vnext/src/runtime/QnxAppStateModule.cpp"
        "${root}/native/lifecycle/QtApplicationLifecycle.cpp"
        PROPERTIES COMPILE_OPTIONS "-fsanitize=address,undefined;-fno-sanitize-recover=undefined;-fno-omit-frame-pointer")
      target_link_options(rnqnx_react_host INTERFACE -fsanitize=address,undefined)
    endif()
  endif()
  if(NOT RNQNX_BUILD_APPSTATE_TESTS)
    return()
  endif()
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "AppState runtime tests require a Linux native BUILD_TESTING graph")
  endif()
  add_executable(rnqnx_appstate_runtime_test "${root}/tests/native/appstate/RuntimeTest.cpp")
  target_link_libraries(rnqnx_appstate_runtime_test PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_appstate_runtime_test)
  set_target_properties(rnqnx_appstate_runtime_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_APPSTATE_SANITIZERS)
    target_compile_options(rnqnx_appstate_runtime_test PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(rnqnx_appstate_runtime_test PRIVATE -fsanitize=address,undefined)
  endif()
  foreach(platform qnx marathon)
    set(config "metro.public.config.js")
    if(platform STREQUAL "marathon")
      set(config "metro.public.marathon.config.js")
    endif()
    set(bundle "${CMAKE_BINARY_DIR}/rnqnx-appstate-${platform}.bundle.js")
    add_custom_command(OUTPUT "${bundle}"
      COMMAND "${root}/node_modules/.bin/metro" build appstate-probe.js --config "${config}"
        --platform "${platform}" --dev false --max-workers 2 --out "${bundle}"
      WORKING_DIRECTORY "${root}/apps/rn-playground"
      DEPENDS "${root}/apps/rn-playground/appstate-probe.js"
        "${root}/apps/rn-playground/${config}"
        "${root}/apps/rn-playground/babel.config.js"
        "${root}/packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js"
        "${root}/pnpm-lock.yaml"
      VERBATIM)
    add_custom_target(rnqnx_appstate_${platform}_bundle DEPENDS "${bundle}")
    add_dependencies(rnqnx_appstate_runtime_test rnqnx_appstate_${platform}_bundle)
    add_test(NAME rnqnx_appstate_${platform} COMMAND rnqnx_appstate_runtime_test "${bundle}" "${platform}")
    set_tests_properties(rnqnx_appstate_${platform} PROPERTIES TIMEOUT 30
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
  endforeach()
endfunction()
