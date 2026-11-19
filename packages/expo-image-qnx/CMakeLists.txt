# 3.20 rather than the repository's 3.31.9: the top-level build enforces its
# own minimum, and the host test compiles this package on CI runners with 3.31.6.
cmake_minimum_required(VERSION 3.20)

# Registers the ExpoImage module and the ExpoImage view config with the QNX
# Expo host. The view itself is the Fabric component ViewManagerAdapter_ExpoImage
# built by RNQNX_ENABLE_QT_EXPO_IMAGE in the renderer; this package only tells
# expo-image's JavaScript what that view accepts.
add_library(expo_image_qnx STATIC
  cpp/ExpoImageQnx.cpp
  cpp/ExpoImageRefs.cpp
  cpp/ExpoImageSupport.cpp
  cpp/ExpoImageViewCommands.cpp)
target_link_libraries(expo_image_qnx PUBLIC rnqnx_expo_modules_qnx)
target_compile_features(expo_image_qnx PUBLIC cxx_std_20)
if(COMMAND rnqnx_enable_strict_warnings)
  rnqnx_enable_strict_warnings(expo_image_qnx)
endif()
