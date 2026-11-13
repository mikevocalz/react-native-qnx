#!/usr/bin/env bash
# Builds native/webgpu's rnqnx_webgpu_test against a Dawn build made by
# build-dawn.sh. Same environment as build-dawn.sh.
#   scripts/webgpu/build-testapp.sh <dawn-src-dir> <dawn-build-dir> <out-dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
src="${1:?usage: build-testapp.sh <dawn-src-dir> <dawn-build-dir> <out-dir>}"
dawn_build="${2:?}"
out="${3:?}"
: "${QNX_TARGET:?source qnxsdp-env.sh first}"

cmake -G Ninja -S "$root/native/webgpu" -B "$out" \
  -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/qnx-8.0.cmake" \
  -DQNX_TARGET_ARCH=aarch64 -DCMAKE_BUILD_TYPE=Release \
  -DRNQNX_DAWN_SOURCE_DIR="$src" -DRNQNX_DAWN_BUILD_DIR="$dawn_build" \
  -DRNQNX_WEBGPU_LIBCXX_SHIM="${RNQNX_WEBGPU_LIBCXX_SHIM:-OFF}"
ninja -C "$out" rnqnx_webgpu_test
echo "built: $out/rnqnx_webgpu_test"
