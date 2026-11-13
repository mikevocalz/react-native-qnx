cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(RnqnxQtNativeUiTests LANGUAGES CXX)
  include(CTest)
endif()

# Loads the real drawer, tab bar and ExpoImage items in an offscreen
# QQuickWindow and drives them with Qt pointer and key events. Host evidence
# only: no Fabric, no React Native, no QNX.
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick Test Network)
set(RNQNX_NATIVE_UI_QML_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../native/qt/nativeui")
cmake_path(NORMAL_PATH RNQNX_NATIVE_UI_QML_DIR)
set(RNQNX_VNEXT "${CMAKE_CURRENT_LIST_DIR}/../../../vnext")
cmake_path(NORMAL_PATH RNQNX_VNEXT)

# The ExpoImage view and its loader pieces, built once for every test target.
add_library(rnqnx_expo_image_under_test OBJECT
  "${RNQNX_VNEXT}/include/react-native-qnx/views/image/ExpoImageItem.h"
  "${RNQNX_VNEXT}/src/views/qt/image/ExpoImageItem.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/ImageDecode.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/ImageDiskCache.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/ImageEffects.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/ImageLayout.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/BlurHash.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/ThumbHash.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/QtImageFetcher.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/ImageFetchQueue.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/ImageRefRegistry.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/ImageLoadedNotifier.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/BlurHashEncoder.cpp"
  "${RNQNX_VNEXT}/src/views/qt/image/ThumbHashEncoder.cpp")
set_target_properties(rnqnx_expo_image_under_test PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_features(rnqnx_expo_image_under_test PUBLIC cxx_std_20)
target_include_directories(rnqnx_expo_image_under_test PUBLIC "${RNQNX_VNEXT}/include")
target_compile_definitions(rnqnx_expo_image_under_test PUBLIC QT_NO_KEYWORDS)
target_link_libraries(rnqnx_expo_image_under_test PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(rnqnx_expo_image_under_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

function(rnqnx_native_ui_test name source)
  add_executable(${name} ${source} $<TARGET_OBJECTS:rnqnx_expo_image_under_test>)
  set_target_properties(${name} PROPERTIES AUTOMOC ON)
  target_compile_features(${name} PRIVATE cxx_std_20)
  target_include_directories(${name} PRIVATE "${RNQNX_VNEXT}/include")
  target_compile_definitions(${name} PRIVATE QT_NO_KEYWORDS
    RNQNX_NATIVE_UI_QML_DIR="${RNQNX_NATIVE_UI_QML_DIR}")
  target_link_libraries(${name} PRIVATE rnqnx_expo_image_under_test Qt6::Test)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME ${name} COMMAND ${name} -platform offscreen)
    set_tests_properties(${name} PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QUICK_BACKEND=software")
  endif()
endfunction()

rnqnx_native_ui_test(rnqnx_qt_native_ui_test NativeUiQuickTest.cpp)
rnqnx_native_ui_test(rnqnx_image_units_test ImageUnitsTest.cpp)
rnqnx_native_ui_test(rnqnx_expo_image_item_test ExpoImageItemTest.cpp)
rnqnx_native_ui_test(rnqnx_image_fetch_queue_test ImageFetchQueueTest.cpp)

# BlurHash and ThumbHash against the reference implementations. The vectors in
# tests/fixtures/image-hashes were produced by blurhash@2.0.5 and thumbhash@0.1.1
# from npm (see "producedBy" in the file).
rnqnx_native_ui_test(rnqnx_image_hash_vectors_test ImageHashVectorsTest.cpp)
target_compile_definitions(rnqnx_image_hash_vectors_test PRIVATE
  RNQNX_HASH_VECTORS="${CMAKE_CURRENT_LIST_DIR}/../../fixtures/image-hashes/reference-vectors.json")
