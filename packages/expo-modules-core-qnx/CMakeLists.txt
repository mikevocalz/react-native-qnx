# 3.20 rather than the repository's 3.31.9: the top-level build enforces its
# own minimum, and the host test compiles this package on CI runners with 3.31.6.
cmake_minimum_required(VERSION 3.20)

if(TARGET rnqnx_expo_modules_qnx)
  return()
endif()

if(NOT DEFINED RNQNX_REACT_NATIVE_DIR)
  set(RNQNX_REACT_NATIVE_DIR "${CMAKE_SOURCE_DIR}/node_modules/react-native")
endif()

add_library(rnqnx_expo_modules_qnx STATIC
  cpp/ExpoQnxInstaller.cpp
  cpp/ExpoQnxModuleRegistry.cpp
  cpp/ExpoQnxUuid.cpp
)

target_include_directories(rnqnx_expo_modules_qnx PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/cpp"
  "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi"
)

target_compile_features(rnqnx_expo_modules_qnx PUBLIC cxx_std_20)

# In the full QNX build Hermes provides the canonical `jsi` target (libjsi.a);
# linking it keeps the static link order right for the consumer app. The
# target may be declared after this directory, so the check is a generator
# expression.
target_link_libraries(rnqnx_expo_modules_qnx PUBLIC $<$<TARGET_EXISTS:jsi>:jsi>)

if(COMMAND rnqnx_enable_strict_warnings)
  rnqnx_enable_strict_warnings(rnqnx_expo_modules_qnx)
endif()
