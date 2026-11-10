include_guard(GLOBAL)
option(RNQNX_ENABLE_WORKLETS "Experimental real Worklets 0.13.1 runtime on the Qt owner" OFF)
option(RNQNX_BUILD_WORKLETS_TESTS "Build production-bundle Worklets host tests" OFF)
set(RNQNX_WORKLETS_DIR "${CMAKE_SOURCE_DIR}/node_modules/react-native-worklets" CACHE PATH "Exact react-native-worklets 0.13.1 package directory")
function(rnqnx_configure_worklets)
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  target_compile_definitions(rnqnx_react_host PUBLIC RNQNX_ENABLE_WORKLETS=$<BOOL:${RNQNX_ENABLE_WORKLETS}>)
  if(NOT RNQNX_ENABLE_WORKLETS)
    if(RNQNX_BUILD_WORKLETS_TESTS)
      message(FATAL_ERROR "Worklets runtime tests require RNQNX_ENABLE_WORKLETS=ON")
    endif()
    return()
  endif()
  # This first build lane proves actual library execution on Linux. QNX lean
  # Hermes unpackers/HBC and SDP ABI are independent gates, not host aliases.
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Worklets prototype is native Linux only; QNX bytecode and ABI qualification remain pending")
  endif()
  if(NOT EXISTS "${RNQNX_WORKLETS_DIR}/package.json")
    message(FATAL_ERROR "Supply RNQNX_WORKLETS_DIR for react-native-worklets 0.13.1")
  endif()
  file(READ "${RNQNX_WORKLETS_DIR}/package.json" manifest)
  string(JSON version GET "${manifest}" version)
  string(JSON name GET "${manifest}" name)
  if(NOT version STREQUAL "0.13.1" OR NOT name STREQUAL "react-native-worklets")
    message(FATAL_ERROR "Worklets native prototype is source-audited only for react-native-worklets 0.13.1")
  endif()
  set(source "${CMAKE_BINARY_DIR}/worklets-0.13.1")
  file(COPY "${RNQNX_WORKLETS_DIR}/Common/cpp/" DESTINATION "${source}")
  file(COPY "${RNQNX_WORKLETS_DIR}/LICENSE" DESTINATION "${source}")
  # Upstream non-Android branch assumes Apple's one-argument pthread API.
  # Patch only this audited line in the private build copy, never node_modules.
  set(queue "${source}/worklets/RunLoop/AsyncQueueImpl.cpp")
  file(READ "${queue}" queue_source)
  string(FIND "${queue_source}" "pthread_setname_np(name.c_str());" old_api)
  if(old_api EQUAL -1)
    message(FATAL_ERROR "Worklets 0.13.1 thread-name source changed; re-audit port patch")
  endif()
  string(REPLACE "pthread_setname_np(name.c_str());" "pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());" queue_source "${queue_source}")
  file(WRITE "${queue}" "${queue_source}")
  # libc++ happened to supply these algorithms transitively; libstdc++ does
  # not. Keep the upstream implementation, supply its missing standard header.
  foreach(unit "RunLoop/EventLoop" "Tools/JSISerializer" "Tools/WorkletsJSIUtils")
    set(unit_path "${source}/worklets/${unit}.cpp")
    file(READ "${unit_path}" unit_source)
    set(anchor "#include <worklets/${unit}.h>")
    string(FIND "${unit_source}" "${anchor}" include_anchor)
    if(include_anchor EQUAL -1)
      message(FATAL_ERROR "Worklets ${unit} source changed; re-audit include adaptation")
    endif()
    string(REPLACE "${anchor}" "${anchor}\n#include <algorithm>" unit_source "${unit_source}")
    if(unit STREQUAL "Tools/WorkletsJSIUtils")
      string(PREPEND unit_source "#include <iterator>\n")
    endif()
    file(WRITE "${unit_path}" "${unit_source}")
  endforeach()
  file(GLOB_RECURSE upstream_sources CONFIGURE_DEPENDS "${source}/worklets/*.cpp")
  add_library(rnqnx_worklets STATIC ${upstream_sources}
    "${root}/native/worklets/QtWorkletsScheduler.cpp"
    "${root}/native/worklets/QtWorkletsFrameSource.cpp"
    "${root}/native/worklets/PlatformLogger.cpp"
    "${root}/native/worklets/QnxWorkletsModule.cpp")
  # Upstream systrace header has a continued // comment diagnosed by GCC.
  # Keep diagnostics visible; do not weaken errors in platform-owned source.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(${upstream_sources} PROPERTIES COMPILE_OPTIONS "-Wno-error=comment")
  endif()
  # C++20 still defines these shared_ptr atomic free functions. Preserve the
  # audited upstream semantics; expose their deprecation without making this
  # one upstream unit fatal under libstdc++14's stricter diagnostics.
  set_property(SOURCE "${source}/worklets/SharedItems/SynchronizableDynamic.cpp"
    APPEND PROPERTY COMPILE_OPTIONS "-Wno-error=deprecated-declarations")
  target_include_directories(rnqnx_worklets PUBLIC "${source}" "${root}/native/worklets")
  target_link_libraries(rnqnx_worklets PUBLIC rnqnx_react_native_renderer rnqnx_hermes Qt6::Core Qt6::Quick)
  target_compile_definitions(rnqnx_worklets PUBLIC WORKLETS_VERSION=0.13.1 QT_NO_KEYWORDS)
  target_link_libraries(rnqnx_react_host PUBLIC rnqnx_worklets)
  if(NOT RNQNX_BUILD_WORKLETS_TESTS)
    return()
  endif()
  if(NOT BUILD_TESTING)
    message(FATAL_ERROR "Worklets runtime tests require BUILD_TESTING")
  endif()
  add_executable(rnqnx_worklets_runtime_test "${root}/tests/native/worklets/RuntimeTest.cpp")
  target_link_libraries(rnqnx_worklets_runtime_test PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_worklets_runtime_test)
  set_target_properties(rnqnx_worklets_runtime_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  add_executable(rnqnx_worklets_scheduler_test "${root}/tests/native/worklets/SchedulerTest.cpp")
  target_link_libraries(rnqnx_worklets_scheduler_test PRIVATE rnqnx_worklets)
  rnqnx_enable_strict_warnings(rnqnx_worklets_scheduler_test)
  add_test(NAME rnqnx_worklets_scheduler COMMAND rnqnx_worklets_scheduler_test)
  add_executable(rnqnx_worklets_frames_test "${root}/tests/native/worklets/FrameSourceTest.cpp")
  target_link_libraries(rnqnx_worklets_frames_test PRIVATE rnqnx_worklets)
  rnqnx_enable_strict_warnings(rnqnx_worklets_frames_test)
  add_test(NAME rnqnx_worklets_frames COMMAND rnqnx_worklets_frames_test)
  set_tests_properties(rnqnx_worklets_frames PROPERTIES TIMEOUT 15
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
  foreach(platform qnx marathon)
    set(bundle "${CMAKE_BINARY_DIR}/rnqnx-worklets-${platform}.bundle.js")
    add_custom_command(OUTPUT "${bundle}"
      COMMAND "${CMAKE_COMMAND}" -E env
        "RNQNX_WORKLETS_DIR=${RNQNX_WORKLETS_DIR}" "RNQNX_WORKLETS_PLATFORM=${platform}"
        "${root}/node_modules/.bin/metro" build probe.js --config metro.config.js
        --platform "${platform}" --dev false --max-workers 2 --out "${bundle}"
      WORKING_DIRECTORY "${root}/tests/native/worklets"
      DEPENDS "${root}/tests/native/worklets/probe.js"
        "${root}/tests/native/worklets/metro.config.js"
        "${root}/tests/native/worklets/babel.config.js"
        "${root}/pnpm-lock.yaml"
      VERBATIM)
    add_custom_target(rnqnx_worklets_${platform}_bundle DEPENDS "${bundle}")
    add_dependencies(rnqnx_worklets_runtime_test rnqnx_worklets_${platform}_bundle)
    add_test(NAME rnqnx_worklets_${platform} COMMAND rnqnx_worklets_runtime_test "${bundle}")
    set_tests_properties(rnqnx_worklets_${platform} PROPERTIES TIMEOUT 30
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
  endforeach()
endfunction()
