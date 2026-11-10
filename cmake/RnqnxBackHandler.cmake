include_guard(GLOBAL)
option(RNQNX_ENABLE_BACK_HANDLER "Wire one Qt host window to React Native BackHandler" OFF)
option(RNQNX_BUILD_BACK_HANDLER_TESTS "Build real Hermes BackHandler contract tests" OFF)
option(RNQNX_BACK_HANDLER_SANITIZERS "Instrument BackHandler adapter and test (not prebuilt Qt/RN/Hermes)" OFF)

function(rnqnx_configure_back_handler)
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  target_compile_definitions(rnqnx_react_host PUBLIC RNQNX_ENABLE_BACK_HANDLER=$<BOOL:${RNQNX_ENABLE_BACK_HANDLER}>)
  if(RNQNX_ENABLE_BACK_HANDLER)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|QNX)$")
      message(FATAL_ERROR "Experimental BackHandler supports QNX or Linux native hosts only")
    endif()
    target_sources(rnqnx_react_host PRIVATE "${root}/vnext/src/runtime/QnxBackHandlerModule.cpp")
  endif()
  if(RNQNX_BACK_HANDLER_SANITIZERS)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      message(FATAL_ERROR "BackHandler sanitizer evidence is Linux Clang/GNU only")
    endif()
    if(RNQNX_ENABLE_BACK_HANDLER)
      set_source_files_properties("${root}/vnext/src/runtime/QnxBackHandlerModule.cpp"
        PROPERTIES COMPILE_OPTIONS "-fsanitize=address,undefined;-fno-sanitize-recover=undefined;-fno-omit-frame-pointer")
      target_link_options(rnqnx_react_host INTERFACE -fsanitize=address,undefined)
    endif()
  endif()
  if(NOT RNQNX_BUILD_BACK_HANDLER_TESTS)
    return()
  endif()
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "BackHandler runtime tests require a Linux native BUILD_TESTING graph")
  endif()
  add_executable(rnqnx_back_handler_runtime_test "${root}/tests/native/backhandler/RuntimeTest.cpp")
  target_link_libraries(rnqnx_back_handler_runtime_test PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_back_handler_runtime_test)
  set_target_properties(rnqnx_back_handler_runtime_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_BACK_HANDLER_SANITIZERS)
    target_compile_options(rnqnx_back_handler_runtime_test PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(rnqnx_back_handler_runtime_test PRIVATE -fsanitize=address,undefined)
  endif()
  foreach(platform qnx marathon)
    set(config "metro.public.config.js")
    if(platform STREQUAL "marathon")
      set(config "metro.public.marathon.config.js")
    endif()
    set(bundle "${CMAKE_BINARY_DIR}/rnqnx-back-handler-${platform}.bundle.js")
    add_custom_command(OUTPUT "${bundle}"
      COMMAND "${root}/node_modules/.bin/metro" build backhandler-probe.js --config "${config}"
        --platform "${platform}" --dev false --max-workers 2 --out "${bundle}"
      WORKING_DIRECTORY "${root}/apps/rn-playground"
      DEPENDS "${root}/apps/rn-playground/backhandler-probe.js"
        "${root}/apps/rn-playground/${config}"
        "${root}/apps/rn-playground/babel.config.js"
        "${root}/packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js"
        "${root}/packages/react-native-qnx/Libraries/Utilities/BackHandler.js"
        "${root}/pnpm-lock.yaml"
      VERBATIM)
    add_custom_target(rnqnx_back_handler_${platform}_bundle DEPENDS "${bundle}")
    add_dependencies(rnqnx_back_handler_runtime_test rnqnx_back_handler_${platform}_bundle)
    add_test(NAME rnqnx_back_handler_${platform} COMMAND rnqnx_back_handler_runtime_test "${bundle}" "${platform}")
    set_tests_properties(rnqnx_back_handler_${platform} PROPERTIES TIMEOUT 30
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
  endforeach()
endfunction()
