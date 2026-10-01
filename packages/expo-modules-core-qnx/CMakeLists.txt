cmake_minimum_required(VERSION 3.31.9)

if(NOT DEFINED RNQNX_REACT_NATIVE_DIR)
  set(RNQNX_REACT_NATIVE_DIR "${CMAKE_SOURCE_DIR}/node_modules/react-native")
endif()

add_library(rnqnx_expo_modules_qnx STATIC
  cpp/ExpoQnxInstaller.cpp
  cpp/ExpoQnxModuleRegistry.cpp
  cpp/ExpoQnxModulesHostObject.cpp
)

target_include_directories(rnqnx_expo_modules_qnx PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/cpp"
  "${RNQNX_REACT_NATIVE_DIR}/ReactCommon/jsi"
)

target_compile_features(rnqnx_expo_modules_qnx PUBLIC cxx_std_20)

if(COMMAND rnqnx_enable_strict_warnings)
  rnqnx_enable_strict_warnings(rnqnx_expo_modules_qnx)
endif()
