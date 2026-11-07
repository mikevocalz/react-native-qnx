# React Native's C++ third-party dependencies, fetched at the exact versions
# selected by the installed React Native package. validate:dependencies checks
# these values against that package so an RN upgrade cannot silently drift them.
#
# Versions come from the installed package, not from upstream "latest":
#   folly 2024.11.18.00      node_modules/react-native/scripts/cocoapods/helpers.rb:50
#   fmt 12.1.0               node_modules/react-native/third-party-podspecs/fmt.podspec:11
#   glog 0.3.5               node_modules/react-native/third-party-podspecs/glog.podspec:11
#   double-conversion 1.1.6  node_modules/react-native/third-party-podspecs/DoubleConversion.podspec:11
#   fast_float 8.0.0         node_modules/react-native/third-party-podspecs/fast_float.podspec:11
#
# Compile flags and source lists are ported from React Native's own recipes
# rather than invented:
#   ReactAndroid/src/main/jni/third-party/folly/CMakeLists.txt
#   ReactAndroid/src/main/jni/third-party/glog/CMakeLists.txt
#   ReactAndroid/src/main/jni/third-party/double-conversion/CMakeLists.txt
#   ReactAndroid/src/main/jni/third-party/fmt/CMakeLists.txt
#   ReactAndroid/src/main/jni/third-party/fast_float/CMakeLists.txt
#   ReactCommon/cmake-utils/react-native-flags.cmake
#
# Boost is React Native's trimmed distribution, not upstream boost: 1.4 MB of
# exactly the headers folly needs. folly/json/json.cpp includes
# boost/algorithm/string.hpp, which is what forced it in. React Native's Android
# recipe also compiles boost's context-switching assembly for folly's fibers;
# nothing here uses fibers, so that assembly is not built.

include_guard(GLOBAL)
include(FetchContent)

# We consume sources and headers only. Pointing SOURCE_SUBDIR at a directory
# that does not exist stops FetchContent from configuring the upstream CMake
# projects, several of which still declare pre-3.5 minimums that current CMake
# rejects.
set(RNQNX_NO_UPSTREAM_BUILD "rnqnx-not-a-cmake-project")

set(RNQNX_FOLLY_VERSION "2024.11.18.00")
set(RNQNX_FMT_VERSION "12.1.0")
set(RNQNX_GLOG_VERSION "0.3.5")
set(RNQNX_DOUBLE_CONVERSION_VERSION "1.1.6")
set(RNQNX_FAST_FLOAT_VERSION "8.0.0")
set(RNQNX_BOOST_VERSION "1.84.0")

# ReactAndroid/src/main/jni/third-party/folly/CMakeLists.txt:10-20, minus the
# Android-only entries. FOLLY_HAVE_XSI_STRERROR_R is Bionic-specific and
# FOLLY_HAVE_RECVMMSG is not used by anything we compile.
set(RNQNX_FOLLY_DEFINITIONS
  FOLLY_NO_CONFIG=1
  FOLLY_MOBILE=1
  FOLLY_USE_LIBCPP=1
  FOLLY_CFG_NO_COROUTINES=1
  FOLLY_HAVE_CLOCK_GETTIME=1
  FOLLY_HAVE_PTHREAD=1
)

FetchContent_Declare(rnqnx_folly
  URL "https://github.com/facebook/folly/archive/refs/tags/v${RNQNX_FOLLY_VERSION}.tar.gz"
  URL_HASH SHA256=b2c6879ba8ba625218d1ab9eefcc1611a9003d05bc2cb1a38f3bae21892e5167
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR ${RNQNX_NO_UPSTREAM_BUILD}
)
FetchContent_Declare(rnqnx_fmt
  URL "https://github.com/fmtlib/fmt/archive/refs/tags/${RNQNX_FMT_VERSION}.tar.gz"
  URL_HASH SHA256=ea7de4299689e12b6dddd392f9896f08fb0777ac7168897a244a6d6085043fea
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR ${RNQNX_NO_UPSTREAM_BUILD}
)
FetchContent_Declare(rnqnx_glog
  URL "https://github.com/google/glog/archive/refs/tags/v${RNQNX_GLOG_VERSION}.tar.gz"
  URL_HASH SHA256=7580e408a2c0b5a89ca214739978ce6ff480b5e7d8d7698a2aa92fadc484d1e0
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR ${RNQNX_NO_UPSTREAM_BUILD}
)
FetchContent_Declare(rnqnx_double_conversion
  URL "https://github.com/google/double-conversion/archive/refs/tags/v${RNQNX_DOUBLE_CONVERSION_VERSION}.tar.gz"
  URL_HASH SHA256=6b850acd8e88515764472c52973277744a945f9a769a93968efc42efef64d67a
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR ${RNQNX_NO_UPSTREAM_BUILD}
)
FetchContent_Declare(rnqnx_fast_float
  URL "https://github.com/fastfloat/fast_float/archive/refs/tags/v${RNQNX_FAST_FLOAT_VERSION}.tar.gz"
  URL_HASH SHA256=f312f2dc34c61e665f4b132c0307d6f70ad9420185fa831911bc24408acf625d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR ${RNQNX_NO_UPSTREAM_BUILD}
)

FetchContent_Declare(rnqnx_boost
  URL "https://github.com/react-native-community/boost-for-react-native/archive/refs/tags/v${RNQNX_BOOST_VERSION}.tar.gz"
  URL_HASH SHA256=954a04e9468dee21259937b83a2b21c3985cc96f91ee0152a34a7f0f752093e8
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR ${RNQNX_NO_UPSTREAM_BUILD}
)

# RN's trimmed Boost contains only libc++ configuration. Linux's Qt binary
# SDK uses libstdc++, so supply the missing platform/compiler configuration
# headers from the exact matching upstream Boost.Config release. This does not
# replace the trimmed Boost implementation headers or change the Boost version.
FetchContent_Declare(rnqnx_boost_config
  URL "https://github.com/boostorg/config/archive/refs/tags/boost-${RNQNX_BOOST_VERSION}.tar.gz"
  URL_HASH SHA256=4c45e04b689e79234cf1ef60f966c93755dca2ff433e72db5b4f367f94007aa9
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR ${RNQNX_NO_UPSTREAM_BUILD}
)

FetchContent_MakeAvailable(
  rnqnx_boost_config
  rnqnx_boost
  rnqnx_folly
  rnqnx_fmt
  rnqnx_glog
  rnqnx_double_conversion
  rnqnx_fast_float
)

if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
  include(${CMAKE_CURRENT_LIST_DIR}/QnxThirdPartyPatches.cmake)
endif()

# glog 0.3.5 is autotools-only: its public headers are generated from .h.in and
# it needs a config.h. React Native substitutes the same token set in
# PrepareGlogTask.kt, which is ported verbatim below, and ships the config.h that
# its Android build uses. Both are copied here rather than re-derived.
set(RNQNX_GLOG_EXPORTED "${CMAKE_CURRENT_BINARY_DIR}/glog-exported")
set(RNQNX_GLOG_CONFIG_DIR "${RNQNX_GLOG_EXPORTED}/config")
file(MAKE_DIRECTORY "${RNQNX_GLOG_EXPORTED}/glog" "${RNQNX_GLOG_CONFIG_DIR}")

set(ac_cv_have_unistd_h 1)
set(ac_cv_have_stdint_h 1)
set(ac_cv_have_systypes_h 1)
set(ac_cv_have_inttypes_h 1)
set(ac_cv_have_libgflags 0)
set(ac_google_start_namespace "namespace google {")
set(ac_cv_have_uint16_t 1)
set(ac_cv_have_u_int16_t 1)
set(ac_cv_have___uint16 0)
set(ac_google_end_namespace "}")
set(ac_cv_have___builtin_expect 1)
set(ac_google_namespace google)
set(ac_cv___attribute___noinline "__attribute__ ((noinline))")
set(ac_cv___attribute___noreturn "__attribute__ ((noreturn))")
set(ac_cv___attribute___printf_4_5 "__attribute__((__format__ (__printf__, 4, 5)))")

foreach(header logging raw_logging stl_logging vlog_is_on)
  configure_file(
    "${rnqnx_glog_SOURCE_DIR}/src/glog/${header}.h.in"
    "${RNQNX_GLOG_EXPORTED}/glog/${header}.h"
    @ONLY
  )
endforeach()
configure_file(
  "${rnqnx_glog_SOURCE_DIR}/src/glog/log_severity.h"
  "${RNQNX_GLOG_EXPORTED}/glog/log_severity.h"
  COPYONLY
)
configure_file(
  "${RNQNX_REACT_NATIVE_DIR}/ReactAndroid/src/main/jni/third-party/glog/config.h"
  "${RNQNX_GLOG_CONFIG_DIR}/config.h"
  COPYONLY
)

# ReactAndroid/src/main/jni/third-party/glog/CMakeLists.txt source list and
# flags. HAVE_PREAD and the warning suppressions are React Native's.
add_library(rnqnx_glog STATIC
  "${rnqnx_glog_SOURCE_DIR}/src/demangle.cc"
  "${rnqnx_glog_SOURCE_DIR}/src/logging.cc"
  "${rnqnx_glog_SOURCE_DIR}/src/raw_logging.cc"
  "${rnqnx_glog_SOURCE_DIR}/src/signalhandler.cc"
  "${rnqnx_glog_SOURCE_DIR}/src/symbolize.cc"
  "${rnqnx_glog_SOURCE_DIR}/src/utilities.cc"
  "${rnqnx_glog_SOURCE_DIR}/src/vlog_is_on.cc"
)
target_include_directories(rnqnx_glog SYSTEM PUBLIC "${RNQNX_GLOG_EXPORTED}")
target_include_directories(rnqnx_glog SYSTEM PRIVATE
  "${RNQNX_GLOG_CONFIG_DIR}"
  "${rnqnx_glog_SOURCE_DIR}/src"
)
target_compile_definitions(rnqnx_glog PRIVATE HAVE_PREAD=1 NDEBUG)
target_compile_options(rnqnx_glog PRIVATE -w)

add_library(rnqnx_fast_float INTERFACE)
target_include_directories(rnqnx_fast_float SYSTEM INTERFACE
  "${rnqnx_fast_float_SOURCE_DIR}/include")

add_library(rnqnx_fmt STATIC "${rnqnx_fmt_SOURCE_DIR}/src/format.cc")
target_include_directories(rnqnx_fmt SYSTEM PUBLIC
  "${rnqnx_fmt_SOURCE_DIR}/include")

# double-conversion 1.1.6 ships its sources under src/, but every consumer
# includes them as <double-conversion/...>. React Native's prepareDoubleConversion
# task (ReactAndroid/build.gradle.kts:340-349) restages them under a
# double-conversion/ directory, and this reproduces that layout.
set(RNQNX_DOUBLE_CONVERSION_STAGE
  "${CMAKE_CURRENT_BINARY_DIR}/double-conversion-staged")
file(GLOB RNQNX_DOUBLE_CONVERSION_FILES
  "${rnqnx_double_conversion_SOURCE_DIR}/src/*.h"
  "${rnqnx_double_conversion_SOURCE_DIR}/src/*.cc")
file(COPY ${RNQNX_DOUBLE_CONVERSION_FILES}
  DESTINATION "${RNQNX_DOUBLE_CONVERSION_STAGE}/double-conversion")

add_library(rnqnx_double_conversion STATIC
  "${RNQNX_DOUBLE_CONVERSION_STAGE}/double-conversion/bignum.cc"
  "${RNQNX_DOUBLE_CONVERSION_STAGE}/double-conversion/bignum-dtoa.cc"
  "${RNQNX_DOUBLE_CONVERSION_STAGE}/double-conversion/cached-powers.cc"
  "${RNQNX_DOUBLE_CONVERSION_STAGE}/double-conversion/diy-fp.cc"
  "${RNQNX_DOUBLE_CONVERSION_STAGE}/double-conversion/double-conversion.cc"
  "${RNQNX_DOUBLE_CONVERSION_STAGE}/double-conversion/fast-dtoa.cc"
  "${RNQNX_DOUBLE_CONVERSION_STAGE}/double-conversion/fixed-dtoa.cc"
  "${RNQNX_DOUBLE_CONVERSION_STAGE}/double-conversion/strtod.cc"
)
target_include_directories(rnqnx_double_conversion SYSTEM PUBLIC
  "${RNQNX_DOUBLE_CONVERSION_STAGE}")
target_compile_options(rnqnx_double_conversion PRIVATE -w)

add_library(rnqnx_boost INTERFACE)
target_include_directories(rnqnx_boost SYSTEM INTERFACE
  "${rnqnx_boost_SOURCE_DIR}"
  "${rnqnx_boost_config_SOURCE_DIR}/include")

# Headers, usable on their own by translation units that only read folly types.
add_library(rnqnx_folly INTERFACE)
target_include_directories(rnqnx_folly SYSTEM INTERFACE "${rnqnx_folly_SOURCE_DIR}")
target_compile_definitions(rnqnx_folly INTERFACE ${RNQNX_FOLLY_DEFINITIONS})
target_link_libraries(rnqnx_folly INTERFACE
  rnqnx_glog
  rnqnx_fmt
  rnqnx_fast_float
  rnqnx_double_conversion
  rnqnx_boost
)

# The compiled half. Source list is React Native's folly_runtime target,
# ReactAndroid/src/main/jni/third-party/folly/CMakeLists.txt:26-56, unchanged.
add_library(rnqnx_folly_runtime STATIC
  "${rnqnx_folly_SOURCE_DIR}/folly/Conv.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/Demangle.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/FileUtil.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/Format.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/ScopeGuard.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/SharedMutex.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/String.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/Unicode.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/concurrency/CacheLocality.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/container/detail/F14Table.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/detail/FileUtilDetail.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/detail/Futex.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/detail/SplitStringSimd.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/detail/UniqueInstance.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/hash/SpookyHashV2.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/json/dynamic.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/json/json_pointer.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/json/json.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/lang/CString.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/lang/Exception.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/lang/SafeAssert.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/lang/ToAscii.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/memory/detail/MallocImpl.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/net/NetOps.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/portability/SysUio.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/synchronization/SanitizeThread.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/synchronization/ParkingLot.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/system/AtFork.cpp"
  "${rnqnx_folly_SOURCE_DIR}/folly/system/ThreadId.cpp"
)
target_link_libraries(rnqnx_folly_runtime PUBLIC rnqnx_folly)
if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
  target_link_libraries(rnqnx_folly_runtime PUBLIC socket)
endif()
# Third-party warnings are not ours to fix, and React Native compiles folly with
# its own suppressions rather than clean.
target_compile_options(rnqnx_folly_runtime PRIVATE -w -fexceptions -frtti)

# React Native's own headers, consumed as a system include so that its warnings
# do not fail our -Werror builds.
add_library(rnqnx_react_native_headers INTERFACE)
target_include_directories(rnqnx_react_native_headers SYSTEM INTERFACE
  "${RNQNX_REACT_NATIVE_DIR}/ReactCommon"
  "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi"
  "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/react/renderer/graphics/platform/cxx"
  "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/react/renderer/textlayoutmanager/platform/cxx"
  "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/react/renderer/imagemanager/platform/cxx"
)
target_link_libraries(rnqnx_react_native_headers INTERFACE rnqnx_folly)
