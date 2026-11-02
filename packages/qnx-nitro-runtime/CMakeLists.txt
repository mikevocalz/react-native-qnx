cmake_minimum_required(VERSION 3.31.9)

# QNX platform-specific v1 native adapter: the class + generated bindings are
# QNX-only, while the Nitro core is compiled once in shared CMake.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/RnqnxNitroV1.cmake")
rnqnx_add_nitro_v1_core()

add_library(rnqnx_nitro_qnx STATIC
  cpp/QnxNitroRuntime.cpp
  cpp/QnxPlatformInfo.cpp
  cpp/generated/QnxPlatformInfo.binding.cpp
)
target_include_directories(rnqnx_nitro_qnx PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/cpp")
rnqnx_configure_nitro_v1_adapter(rnqnx_nitro_qnx)
