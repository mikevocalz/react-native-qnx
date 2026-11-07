# Finite native diagnostic fixture, independent of RN/Qt/Nitro and premium code.
function(rnqnx_add_systems_probe)
  if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
    message(FATAL_ERROR "RNQNX_BUILD_QNX_SYSTEMS_PROBES requires Linux, Darwin or QNX")
  endif()
  find_package(Threads REQUIRED)
  set(probe_source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/native/qnx-systems")
  add_executable(rnqnx_systems_probe
    "${probe_source}/main.cpp"
    "${probe_source}/ThreadsProbe.cpp"
    "${probe_source}/ShortIoProbe.cpp")
  target_compile_features(rnqnx_systems_probe PRIVATE cxx_std_20)
  target_link_libraries(rnqnx_systems_probe PRIVATE Threads::Threads)
  target_compile_options(rnqnx_systems_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
  set_target_properties(rnqnx_systems_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    target_compile_definitions(rnqnx_systems_probe PRIVATE _QNX_SOURCE)
  elseif(RNQNX_ENABLE_HOST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rnqnx_systems_probe PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(rnqnx_systems_probe PRIVATE -fsanitize=address,undefined)
  endif()
  if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME rnqnx_systems_probe COMMAND rnqnx_systems_probe)
    set_tests_properties(rnqnx_systems_probe PROPERTIES
      TIMEOUT 10
      PASS_REGULAR_EXPRESSION "RNQNX_SYSTEMS_OK threads=2 tls-destructors=2 short-read=14 eof=1 cleanup=1"
      FAIL_REGULAR_EXPRESSION "RNQNX_SYSTEMS_FAIL;AddressSanitizer;UndefinedBehaviorSanitizer;runtime error:")
    foreach(fault IN ITEMS tls-mismatch early-eof)
      add_test(NAME "rnqnx_systems_negative_${fault}" COMMAND "${CMAKE_COMMAND}"
        "-DPROBE=$<TARGET_FILE:rnqnx_systems_probe>" "-DFAULT=${fault}"
        -P "${probe_source}/ExpectFailure.cmake")
      set_tests_properties("rnqnx_systems_negative_${fault}" PROPERTIES TIMEOUT 12)
    endforeach()
  endif()
endfunction()
