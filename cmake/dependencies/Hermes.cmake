# Hermes host integration for the exact source line selected by React Native.
#
# React Native 0.88.1 records hermes-v260318099.0.5 in
# node_modules/react-native/sdks/.hermesv1version. The npm compiler package is
# the matching 260318099.0.5 release, so source and compiler identity
# are validated separately.

include_guard(GLOBAL)
include(FetchContent)

set(RNQNX_HERMES_EXPECTED_TAG "hermes-v260318099.0.5")
set(RNQNX_HERMES_EXPECTED_COMMIT "9a115f15785851f4ff2747ebb090ddd066fc8ad2")

set(_rnqnx_hermes_version_file
  "${RNQNX_REACT_NATIVE_DIR}/sdks/.hermesv1version")
if(NOT EXISTS "${_rnqnx_hermes_version_file}")
  message(FATAL_ERROR
    "React Native Hermes source identity is missing: ${_rnqnx_hermes_version_file}")
endif()
file(READ "${_rnqnx_hermes_version_file}" RNQNX_HERMES_RN_TAG)
string(STRIP "${RNQNX_HERMES_RN_TAG}" RNQNX_HERMES_RN_TAG)
if(NOT RNQNX_HERMES_RN_TAG STREQUAL RNQNX_HERMES_EXPECTED_TAG)
  message(FATAL_ERROR
    "React Native selects ${RNQNX_HERMES_RN_TAG}, but react-native-qnx is "
    "audited for ${RNQNX_HERMES_EXPECTED_TAG}. Re-audit before building.")
endif()

FetchContent_Declare(rnqnx_hermes
  GIT_REPOSITORY "https://github.com/facebook/hermes.git"
  GIT_TAG "${RNQNX_HERMES_EXPECTED_COMMIT}"
  GIT_SHALLOW FALSE
  GIT_PROGRESS TRUE
  SOURCE_SUBDIR ${RNQNX_NO_UPSTREAM_BUILD}
)
FetchContent_MakeAvailable(rnqnx_hermes)

execute_process(
  COMMAND git -C "${rnqnx_hermes_SOURCE_DIR}" rev-parse HEAD
  OUTPUT_VARIABLE RNQNX_HERMES_ACTUAL_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE RNQNX_HERMES_REV_PARSE_RESULT
)
if(NOT RNQNX_HERMES_REV_PARSE_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not read the fetched Hermes commit")
endif()
if(NOT RNQNX_HERMES_ACTUAL_COMMIT STREQUAL RNQNX_HERMES_EXPECTED_COMMIT)
  message(FATAL_ERROR
    "Hermes resolved to ${RNQNX_HERMES_ACTUAL_COMMIT}, expected "
    "${RNQNX_HERMES_EXPECTED_COMMIT}.")
endif()

set(HERMES_ENABLE_INTL OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_TEST_SUITE OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_DEBUGGER ON CACHE BOOL "" FORCE)
set(HERMES_BUILD_APPLE_FRAMEWORK OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_TOOLS ON CACHE BOOL "" FORCE)
set(JSI_DIR "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi" CACHE PATH "" FORCE)

add_subdirectory(
  "${rnqnx_hermes_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}/hermes"
  EXCLUDE_FROM_ALL
)

add_library(rnqnx_hermes INTERFACE)
target_link_libraries(rnqnx_hermes INTERFACE hermesvm)
target_include_directories(rnqnx_hermes SYSTEM INTERFACE
  "${rnqnx_hermes_SOURCE_DIR}/API"
  "${rnqnx_hermes_SOURCE_DIR}/public"
)
