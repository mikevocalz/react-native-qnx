cmake_minimum_required(VERSION 3.28)
project(react-native-qnx VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RNQNX_BUILD_SCREEN_DEMO "Build the minimal QNX Screen demo executable" ON)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
  message(WARNING "This project is designed to be cross-compiled for QNX. "
                  "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-8.0.cmake")
endif()

# Minimal Milestone 0 target: native QNX Screen demo executable.
if(RNQNX_BUILD_SCREEN_DEMO)
  add_executable(rnqnx_playground
    native/application/main.cpp
    native/screen/QnxScreenSurface.cpp
  )

  target_include_directories(rnqnx_playground PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/native/screen
  )

  target_link_libraries(rnqnx_playground PRIVATE
    screen
  )

  # Place the binary next to the build directory for easy deployment.
  set_target_properties(rnqnx_playground PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endif()

# Future targets (Milestone 1+) will be added here: Hermes, JSI, Fabric, renderer, etc.
