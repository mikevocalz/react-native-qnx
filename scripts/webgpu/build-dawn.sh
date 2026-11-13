#!/usr/bin/env bash
# Cross-compiles Dawn (OpenGL ES backend + Tint GLSL writer, static monolithic
# library) for QNX SDP 8.0 aarch64le.
#
# Run it inside the qnx805-build container (docs/handoff/QNX_SDK_CONTAINER_MACOS.md),
# or any Linux shell with the SDP sourced:
#   source /work/qnx800/qnxsdp-env.sh
#   scripts/webgpu/build-dawn.sh <dawn-src-dir> <build-dir> [jobs]
#
# Needs network access on first configure (Dawn fetches its third_party sources)
# and about 20 GB of free disk space. The Mac host does not have that: use the
# container's own filesystem (for example /opt/s5), not /work.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
src="${1:?usage: build-dawn.sh <dawn-src-dir> <build-dir> [jobs]}"
build="${2:?usage: build-dawn.sh <dawn-src-dir> <build-dir> [jobs]}"
jobs="${3:-$(nproc)}"
: "${QNX_TARGET:?source qnxsdp-env.sh first}"

# Vulkan is off on purpose: the board's Vulkan driver is 1.0.182 and Dawn
# requires 1.1 (see docs/v2/webgpu-qnx.md). Tint's SPIR-V reader/writer and the
# SPIR-V validator are only needed by that backend.
cmake -G Ninja -S "$src" -B "$build" \
  -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/qnx-8.0.cmake" \
  -DQNX_TARGET_ARCH=aarch64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DDAWN_FETCH_DEPENDENCIES=ON \
  -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC \
  -DDAWN_ENABLE_VULKAN=OFF \
  -DDAWN_ENABLE_OPENGLES=ON \
  -DDAWN_ENABLE_DESKTOP_GL=OFF \
  -DDAWN_ENABLE_NULL=ON \
  -DDAWN_ENABLE_SPIRV_VALIDATION=OFF \
  -DDAWN_ENABLE_SWIFTSHADER=OFF \
  -DDAWN_ENABLE_INSTALL=OFF \
  -DDAWN_USE_X11=OFF -DDAWN_USE_WAYLAND=OFF -DDAWN_USE_GLFW=OFF \
  -DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_TESTS=OFF -DDAWN_BUILD_BENCHMARKS=OFF \
  -DDAWN_BUILD_PROTOBUF=OFF \
  -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_CMD_TOOLS=OFF -DTINT_BUILD_IR_BINARY=OFF \
  -DTINT_BUILD_SPV_READER=OFF -DTINT_BUILD_SPV_WRITER=OFF \
  -DTINT_BUILD_GLSL_WRITER=ON -DTINT_BUILD_GLSL_VALIDATOR=OFF

ninja -C "$build" -j "$jobs" webgpu_dawn
echo "built: $(find "$build" -name 'libwebgpu_dawn.a' | head -1)"
