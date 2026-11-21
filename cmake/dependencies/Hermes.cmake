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
# Hermes and its consumers must share JSI's exception type identities across
# dynamic-library boundaries. Static copies give Darwin duplicate JSError RTTI
# and turn a JavaScript exception into the scheduler's non-JS fallback.
set(HERMES_BUILD_SHARED_JSI ON CACHE BOOL "" FORCE)
set(HERMES_ENABLE_TOOLS ON CACHE BOOL "" FORCE)
set(JSI_DIR "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi" CACHE PATH "" FORCE)

# llvh raw_fd_ostream::seek calls ::lseek64 when HAVE_LSEEK64 is set. musl
# (Alpine) declares lseek64 only under _LARGEFILE64_SOURCE, so the configure
# probe says yes and the C++ compile then fails. On LP64 Linux off_t is already
# 64 bits in glibc and musl, so plain lseek is equivalent: skip the probe.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(HAVE_LSEEK64 OFF CACHE INTERNAL "off_t is 64-bit on LP64 Linux" FORCE)
endif()

# Hermes defaults HERMES_SLOW_DEBUG to ON, and in a Debug configuration that
# compiles heap and card-table validation into every young-generation
# collection. The host Debug graph carried it (HERMES_SLOW_DEBUG:BOOL=ON in the
# evidence caches), and docs/validation/pr161-layout-dispatch-2026-11-16.md
# measured 97.3 percent of corpus sample time inside the Hermes VM. Force it off.
# rnqnx_hermes_build_mode fails if the VM still reports Build "SlowDebug".
#
# A Debug configuration also has no optimisation level of its own, so the Hermes
# VM was compiled at -O0. Append -O2 for the Hermes subtree only: the variables
# are read when the directory finishes, so they are restored right after it and
# this repository's own targets keep the flags they were configured with.
# -O2 does not define NDEBUG, so Hermes keeps its Debug assertions.
set(HERMES_SLOW_DEBUG OFF CACHE BOOL
  "Hermes slow debug checks: forced off by react-native-qnx" FORCE)
set(_rnqnx_saved_c_flags "${CMAKE_C_FLAGS}")
set(_rnqnx_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
string(APPEND CMAKE_C_FLAGS " -O2")
string(APPEND CMAKE_CXX_FLAGS " -O2")

add_subdirectory(
  "${rnqnx_hermes_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}/hermes"
  EXCLUDE_FROM_ALL
)

set(CMAKE_C_FLAGS "${_rnqnx_saved_c_flags}")
set(CMAKE_CXX_FLAGS "${_rnqnx_saved_cxx_flags}")
unset(_rnqnx_saved_c_flags)
unset(_rnqnx_saved_cxx_flags)

add_library(rnqnx_hermes INTERFACE)
target_link_libraries(rnqnx_hermes INTERFACE hermesvm)
target_include_directories(rnqnx_hermes SYSTEM INTERFACE
  "${rnqnx_hermes_SOURCE_DIR}/API"
  "${rnqnx_hermes_SOURCE_DIR}/public"
)
