cmake_minimum_required(VERSION 3.24)
project(rnqnx_qt_rnsvg_image_http_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Real local HTTP through QtImageCache into an Image element's loader. Like
# tests/image-loader it builds the loader sources directly and needs only Qt.
# `-DRNQNX_QT_RNSVG_SANITIZERS=ON` instruments everything with ASan and UBSan
# (Linux Clang or GCC).
option(RNQNX_QT_RNSVG_SANITIZERS "Instrument the renderer and this test with ASan/UBSan" OFF)
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Network Quick Test)

set(RNQNX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
add_subdirectory("${RNQNX_ROOT}/native/qt/rnsvg" "${CMAKE_CURRENT_BINARY_DIR}/rnsvg")

add_executable(rnqnx_qt_rnsvg_image_http_test
  RnsvgImageHttpTest.cpp
  "${RNQNX_ROOT}/vnext/src/runtime/QtImageCache.cpp"
  "${RNQNX_ROOT}/vnext/src/fabric/QtRnsvgImageProvider.cpp")
set_target_properties(rnqnx_qt_rnsvg_image_http_test PROPERTIES AUTOMOC ON)
target_include_directories(rnqnx_qt_rnsvg_image_http_test PRIVATE "${RNQNX_ROOT}/vnext/include")
target_compile_definitions(rnqnx_qt_rnsvg_image_http_test PRIVATE QT_NO_KEYWORDS)
target_link_libraries(rnqnx_qt_rnsvg_image_http_test PRIVATE rnqnx_qt_rnsvg Qt6::Network Qt6::Test)
set(flags -Wall -Wextra -Wpedantic -Werror)
if(RNQNX_QT_RNSVG_SANITIZERS)
  list(APPEND flags -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  target_compile_options(rnqnx_qt_rnsvg PRIVATE ${flags})
  target_link_options(rnqnx_qt_rnsvg_image_http_test PRIVATE -fsanitize=address,undefined)
endif()
target_compile_options(rnqnx_qt_rnsvg_image_http_test PRIVATE ${flags})
enable_testing()
add_test(NAME rnqnx_qt_rnsvg_image_http_test COMMAND rnqnx_qt_rnsvg_image_http_test -platform offscreen)
set_tests_properties(rnqnx_qt_rnsvg_image_http_test PROPERTIES TIMEOUT 120
  ENVIRONMENT "QT_QUICK_BACKEND=software;ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1")
