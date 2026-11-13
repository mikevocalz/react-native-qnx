#!/usr/bin/env bash
# Builds Dawn with LLVM clang against the QNX SDP sysroot (the path the Pi 400
# device run used). Compiles and archives only; link the test app in the
# qnx805-build container with build-testapp.sh and RNQNX_WEBGPU_LIBCXX_SHIM=ON.
#
#   export RNQNX_LLVM_BIN=/opt/homebrew/opt/llvm/bin     # clang, clang++, llvm-ar, llvm-ranlib
#   export RNQNX_QNX_TARGET=<sdp>/target/qnx
#   scripts/webgpu/build-dawn-clang.sh <dawn-src-dir> <build-dir> [jobs]
#
# Fetch the source first (fetch-dawn.sh) and let Dawn fetch its third_party
# dependencies by leaving DAWN_FETCH_DEPENDENCIES=ON (the default here; it needs
# git and network). Set it to OFF when the tree already has them.
# The build tree can sit on a RAM disk: it stays under 1 GB at -O1.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
src="${1:?usage: build-dawn-clang.sh <dawn-src-dir> <build-dir> [jobs]}"
build="${2:?usage: build-dawn-clang.sh <dawn-src-dir> <build-dir> [jobs]}"
jobs="${3:-6}"
: "${RNQNX_LLVM_BIN:?set RNQNX_LLVM_BIN}"
: "${RNQNX_QNX_TARGET:?set RNQNX_QNX_TARGET}"
export RNQNX_LLVM_BIN RNQNX_QNX_TARGET

cmake -G Ninja -S "$src" -B "$build" \
  -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/qnx-8.0-clang.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS_RELEASE="-O1 -DNDEBUG -g0" -DCMAKE_C_FLAGS_RELEASE="-O1 -DNDEBUG -g0" \
  -DCMAKE_CXX_STANDARD=20 \
  -DDAWN_FETCH_DEPENDENCIES="${DAWN_FETCH_DEPENDENCIES:-ON}" \
  -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC \
  -DDAWN_ENABLE_VULKAN=OFF -DDAWN_ENABLE_OPENGLES=ON -DDAWN_ENABLE_DESKTOP_GL=OFF \
  -DDAWN_ENABLE_NULL=ON -DDAWN_ENABLE_SPIRV_VALIDATION=OFF -DDAWN_ENABLE_SWIFTSHADER=OFF \
  -DDAWN_ENABLE_INSTALL=OFF -DDAWN_USE_X11=OFF -DDAWN_USE_WAYLAND=OFF -DDAWN_USE_GLFW=OFF \
  -DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_TESTS=OFF -DDAWN_BUILD_BENCHMARKS=OFF \
  -DDAWN_BUILD_PROTOBUF=OFF \
  -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_CMD_TOOLS=OFF -DTINT_BUILD_IR_BINARY=OFF \
  -DTINT_BUILD_SPV_READER=OFF -DTINT_BUILD_SPV_WRITER=OFF \
  -DTINT_BUILD_GLSL_WRITER=ON -DTINT_BUILD_GLSL_VALIDATOR=OFF

ninja -C "$build" -j "$jobs" webgpu_dawn
echo "built: $(find "$build" -name 'libwebgpu_dawn.a' | head -1)"
