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

option(RNQNX_ENABLE_EXPO_APPLICATION "Expose real Qt application metadata through ExpoApplication" OFF)
target_compile_definitions(rnqnx_expo_modules_qnx PUBLIC RNQNX_ENABLE_EXPO_APPLICATION=$<BOOL:${RNQNX_ENABLE_EXPO_APPLICATION}>)
if(RNQNX_ENABLE_EXPO_APPLICATION)
  if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|QNX)$")
    message(FATAL_ERROR "ExpoApplication supports QNX or Linux native hosts only")
  endif()
  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui)
  target_sources(rnqnx_expo_modules_qnx PRIVATE cpp/ExpoQnxApplication.cpp cpp/ExpoQnxApplicationMetadata.cpp)
  target_link_libraries(rnqnx_expo_modules_qnx PUBLIC Qt6::Core Qt6::Gui)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/RnqnxExpoConstants.cmake")
rnqnx_configure_expo_constants(rnqnx_expo_modules_qnx)
