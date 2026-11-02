cmake_minimum_required(VERSION 3.31.9)

# Linux-only native MarathonOS adapter. Builds and installs NO QNX binary
# artifact and never links the QNX platform/Screen library.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "rnqnx_nitro_marathon is Linux-only")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/RnqnxNitroV1.cmake")
rnqnx_add_nitro_v1_core()

add_library(rnqnx_nitro_marathon STATIC
  MarathonNitroRuntime.cpp
  MarathonPlatformInfo.cpp
  generated/MarathonPlatformInfo.binding.cpp
)
target_include_directories(rnqnx_nitro_marathon PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}")
rnqnx_configure_nitro_v1_adapter(rnqnx_nitro_marathon)
