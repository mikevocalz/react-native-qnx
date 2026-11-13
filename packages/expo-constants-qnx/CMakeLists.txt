# 3.20 rather than the repository's 3.31.9: the top-level build enforces its
# own minimum, and the host test compiles this package on CI runners with 3.31.6.
cmake_minimum_required(VERSION 3.20)

# Registers ExponentConstants with the QNX Expo host. Added to an app by
# RnqnxConsumerApp.cmake when the app depends on this package (expoModules in
# native-app.json); the Expo host library is rnqnx_expo_modules_qnx.
add_library(expo_constants_qnx STATIC cpp/ExpoConstantsQnx.cpp)
target_link_libraries(expo_constants_qnx PUBLIC rnqnx_expo_modules_qnx)
target_compile_features(expo_constants_qnx PUBLIC cxx_std_20)
if(COMMAND rnqnx_enable_strict_warnings)
  rnqnx_enable_strict_warnings(expo_constants_qnx)
endif()
