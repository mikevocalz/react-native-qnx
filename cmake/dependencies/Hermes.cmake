# Hermes, built from source at the version React Native 0.86.2 selects.
#
# Which Hermes: the installed package carries two versions, and the newer one
# is the default. `sdks/hermes-engine/version.properties:1-2` declares
# HERMES_VERSION_NAME=0.17.0 and HERMES_V1_VERSION_NAME=250829098.0.16, and
# every build path opts *out* of v1 rather than into it:
#   - Apple:   sdks/hermes-engine/hermes-utils.rb:89-91 — v1 unless
#              RCT_HERMES_V1_ENABLED == "0"
#   - Android: @react-native/gradle-plugin PrivateReactExtension.kt:62 —
#              convention(true)
#   - package.json:179 pins hermes-compiler 250829098.0.16, matching v1
# So `hermes-v250829098.0.16` is React Native 0.86.2's Hermes. The 0.17.0 tag
# is the legacy opt-out path and is not built here.
#
# Nothing usable ships in the package: sdks/hermes and sdks/hermesc are absent,
# and ReactCommon/hermes contains only React Native's glue, which includes
# <hermes/...> and needs this tree present.
#
# Why a git tag rather than a tarball: GitHub's generated tarballs are not
# byte-reproducible, so a recorded SHA256 would eventually fail for reasons that
# have nothing to do with the source changing. The tag is pinned to its commit
# and verified below instead.

include_guard(GLOBAL)
include(FetchContent)

set(RNQNX_HERMES_TAG "hermes-v250829098.0.16")
# `git ls-remote --tags https://github.com/facebook/hermes.git`, 2026-08-14.
set(RNQNX_HERMES_COMMIT "90f23852efcd361315688e2904d2446707fa274c")

# Hermes declares cmake_minimum_required(VERSION 3.21.0) at CMakeLists.txt:6.
if(CMAKE_VERSION VERSION_LESS 3.21.0)
  message(FATAL_ERROR "Hermes requires CMake 3.21.0 or newer")
endif()

FetchContent_Declare(rnqnx_hermes
  GIT_REPOSITORY "https://github.com/facebook/hermes.git"
  GIT_TAG "${RNQNX_HERMES_TAG}"
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
  # Hermes is configured explicitly below, not by add_subdirectory, so that its
  # options are set before its CMakeLists runs.
  SOURCE_SUBDIR ${RNQNX_NO_UPSTREAM_BUILD}
)
FetchContent_MakeAvailable(rnqnx_hermes)

# A tag can be moved. Pinning the commit is the part that actually holds.
execute_process(
  COMMAND git -C "${rnqnx_hermes_SOURCE_DIR}" rev-parse HEAD
  OUTPUT_VARIABLE RNQNX_HERMES_ACTUAL_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE RNQNX_HERMES_REV_PARSE_RESULT
)
if(NOT RNQNX_HERMES_REV_PARSE_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not read the fetched Hermes commit")
endif()
if(NOT RNQNX_HERMES_ACTUAL_COMMIT STREQUAL RNQNX_HERMES_COMMIT)
  message(FATAL_ERROR
    "Hermes tag ${RNQNX_HERMES_TAG} resolved to ${RNQNX_HERMES_ACTUAL_COMMIT}, "
    "expected ${RNQNX_HERMES_COMMIT}. The tag moved; do not build it blind.")
endif()

# Options, with the reason for each. Defaults in this tree differ from 0.17.0 —
# the debugger and the Apple framework both default OFF here — so everything we
# depend on is set explicitly rather than inherited.
#
# Intl is off for v1. React Native's own builds force it on
# (ReactAndroid/hermes-engine/build.gradle.kts:358,
# sdks/hermes-engine/utils/build-apple-framework.sh:103) because they ship ICU;
# QNX has no ICU story yet, so enabling it here would build a dependency the
# target cannot honour. Recorded as an open decision, not a silent default.
set(HERMES_ENABLE_INTL OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_TEST_SUITE OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_DEBUGGER ON CACHE BOOL "" FORCE)
set(HERMES_BUILD_APPLE_FRAMEWORK OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_TOOLS ON CACHE BOOL "" FORCE)

# Hermes compiles against React Native's JSI copy, not its own bundled one, so
# that everything linking React Native sees one jsi. This is what React Native
# passes on both platforms (sdks/hermes-engine/utils/build-apple-framework.sh:113,
# ReactAndroid/hermes-engine/build.gradle.kts:103).
set(JSI_DIR "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi" CACHE PATH "" FORCE)

# IMPORT_HOST_COMPILERS is deliberately unset. It exists for cross-compiling,
# where a host hermesc must be built first (lib/InternalJavaScript/CMakeLists.txt:6-14).
# A host build uses the hermesc produced in the same build tree. The QNX
# cross-build will need the two-stage bootstrap; that is a G0-gated task.

add_subdirectory("${rnqnx_hermes_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/hermes" EXCLUDE_FROM_ALL)

# Linkage is chosen by target name in this tree, not by BUILD_SHARED_LIBS:
# lib/CMakeLists.txt declares hermesvm (SHARED) and hermesvm_a (STATIC).
add_library(rnqnx_hermes INTERFACE)
target_link_libraries(rnqnx_hermes INTERFACE hermesvm)
target_include_directories(rnqnx_hermes SYSTEM INTERFACE
  "${rnqnx_hermes_SOURCE_DIR}/API"
  "${rnqnx_hermes_SOURCE_DIR}/public"
)
