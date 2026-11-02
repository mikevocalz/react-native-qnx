cmake_minimum_required(VERSION 3.31.9)
project(rnqnx_nitro_v1_host_compile LANGUAGES C CXX)

# A compile-only Linux safety lane for upstream Nitro v1 source + the QNX
# adapter. It is NOT a QNX SDP compile, Hermes runtime or device proof.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(RNQNX_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(RNQNX_REACT_NATIVE_DIR "${RNQNX_REPO_ROOT}/node_modules/react-native")
set(RNQNX_NITRO_ROOT "${RNQNX_REPO_ROOT}/node_modules/react-native-nitro-modules")
set(RNQNX_NITRO_CORE_ROOT "${RNQNX_REPO_ROOT}/node_modules/nitro-core")
add_subdirectory("${RNQNX_REPO_ROOT}/packages/qnx-nitro-runtime"
  "${CMAKE_BINARY_DIR}/nitro-runtime")
