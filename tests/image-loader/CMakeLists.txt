cmake_minimum_required(VERSION 3.24)
project(rnqnx_image_loader_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Network Qml Quick)
add_executable(qnx_image_cache_test
  QtImageCacheTest.cpp
  ../../vnext/src/runtime/QtImageCache.cpp
  ../../vnext/src/views/qt/QnxImageProvider.cpp
  ../../vnext/src/views/qt/QtQuickItemFactory.cpp
  ../../vnext/src/views/qt/QtExtraItems.cpp)
target_include_directories(qnx_image_cache_test PRIVATE ../../vnext/include)
target_link_libraries(qnx_image_cache_test PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick)
target_compile_options(qnx_image_cache_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
enable_testing()
add_test(NAME qnx_image_cache_test COMMAND qnx_image_cache_test -platform offscreen)
