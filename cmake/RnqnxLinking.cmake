include_guard(GLOBAL)
option(RNQNX_ENABLE_LINKING "Enable scoped inbound custom-scheme React Native Linking" OFF)
option(RNQNX_BUILD_LINKING_TESTS "Build real Hermes Linking contract tests" OFF)
option(RNQNX_LINKING_SANITIZERS "Instrument Linking adapter and test (not prebuilt Qt/RN/Hermes)" OFF)

function(rnqnx_configure_linking)
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  target_compile_definitions(rnqnx_react_host PUBLIC RNQNX_ENABLE_LINKING=$<BOOL:${RNQNX_ENABLE_LINKING}>)
  if(RNQNX_ENABLE_LINKING)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|QNX)$")
      message(FATAL_ERROR "Experimental Linking supports QNX or Linux native hosts only")
    endif()
    target_sources(rnqnx_react_host PRIVATE "${root}/vnext/src/runtime/QnxLinkingModule.cpp")
  endif()
  if(RNQNX_LINKING_SANITIZERS)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      message(FATAL_ERROR "Linking sanitizer evidence is Linux Clang/GNU only")
    endif()
    if(RNQNX_ENABLE_LINKING)
      set_source_files_properties("${root}/vnext/src/runtime/QnxLinkingModule.cpp"
        PROPERTIES COMPILE_OPTIONS "-fsanitize=address,undefined;-fno-sanitize-recover=undefined;-fno-omit-frame-pointer")
      target_link_options(rnqnx_react_host INTERFACE -fsanitize=address,undefined)
    endif()
  endif()
  if(NOT RNQNX_BUILD_LINKING_TESTS)
    return()
  endif()
  if(NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Linking runtime tests require a Linux native BUILD_TESTING graph")
  endif()
  add_executable(rnqnx_linking_runtime_test "${root}/tests/native/linking/RuntimeTest.cpp")
  target_link_libraries(rnqnx_linking_runtime_test PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_linking_runtime_test)
  set_target_properties(rnqnx_linking_runtime_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(RNQNX_LINKING_SANITIZERS)
    target_compile_options(rnqnx_linking_runtime_test PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(rnqnx_linking_runtime_test PRIVATE -fsanitize=address,undefined)
  endif()
  foreach(platform qnx marathon)
    set(config "metro.public.config.js")
    if(platform STREQUAL "marathon")
      set(config "metro.public.marathon.config.js")
    endif()
    set(bundle "${CMAKE_BINARY_DIR}/rnqnx-linking-${platform}.bundle.js")
    add_custom_command(OUTPUT "${bundle}"
      COMMAND "${root}/node_modules/.bin/metro" build linking-probe.js --config "${config}"
        --platform "${platform}" --dev false --max-workers 2 --out "${bundle}"
      WORKING_DIRECTORY "${root}/apps/rn-playground"
      DEPENDS "${root}/apps/rn-playground/linking-probe.js"
        "${root}/apps/rn-playground/${config}"
        "${root}/apps/rn-playground/babel.config.js"
        "${root}/packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js"
        "${root}/packages/react-native-qnx/Libraries/Linking/QnxLinking.js"
        "${root}/pnpm-lock.yaml"
      VERBATIM)
    add_custom_target(rnqnx_linking_${platform}_bundle DEPENDS "${bundle}")
    add_dependencies(rnqnx_linking_runtime_test rnqnx_linking_${platform}_bundle)
    add_test(NAME rnqnx_linking_${platform} COMMAND rnqnx_linking_runtime_test "${bundle}" "${platform}")
    set_tests_properties(rnqnx_linking_${platform} PROPERTIES TIMEOUT 30
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
  endforeach()
endfunction()
