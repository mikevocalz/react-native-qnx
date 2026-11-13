#!/usr/bin/env bash
# Builds the Qt Quick composition test (rnqnx_webgpu_qt_compose) for QNX.
# Same environment as build-testapp.sh, plus a cross-built Qt 6 with Quick:
#   QT_PREFIX     cross-built Qt prefix or build tree (default: the public Qt 6.12.0
#                 tree in the qnx805-build container)
#   QT_HOST_PATH  host Qt with moc/rcc (default: that tree's host build)
#   scripts/webgpu/build-qt-compose.sh <dawn-src-dir> <dawn-build-dir> <out-dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
src="${1:?usage: build-qt-compose.sh <dawn-src-dir> <dawn-build-dir> <out-dir>}"
dawn_build="${2:?}"
out="${3:?}"
: "${QNX_TARGET:?source qnxsdp-env.sh first}"
qt_prefix="${QT_PREFIX:-/home/qnxbuild/qt-6.12.0-public/qnx-build/qtbase}"
qt_host="${QT_HOST_PATH:-/home/qnxbuild/qt-6.12.0-public/host-build/qtbase}"

export RNQNX_QT_PREFIX="$qt_prefix"
cmake -G Ninja -S "$root/native/webgpu" -B "$out" \
  -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/qnx-8.0-qt.cmake" \
  -DQNX_TARGET_ARCH=aarch64 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$qt_prefix" -DQT_HOST_PATH="$qt_host" \
  -DCMAKE_LIBRARY_PATH="$QNX_TARGET/aarch64le/usr/lib" \
  -DHAVE_EGL=ON -DHAVE_GLESv2=ON \
  -DEGL_LIBRARY="$QNX_TARGET/aarch64le/usr/lib/libEGL.so" -DEGL_INCLUDE_DIR="$QNX_TARGET/usr/include" \
  -DGLESv2_LIBRARY="$QNX_TARGET/aarch64le/usr/lib/libGLESv2.so" -DGLESv2_INCLUDE_DIR="$QNX_TARGET/usr/include" \
  -DRNQNX_DAWN_SOURCE_DIR="$src" -DRNQNX_DAWN_BUILD_DIR="$dawn_build" \
  -DRNQNX_WEBGPU_LIBCXX_SHIM="${RNQNX_WEBGPU_LIBCXX_SHIM:-OFF}" \
  -DRNQNX_WEBGPU_QT_COMPOSE=ON
ninja -C "$out" rnqnx_webgpu_qt_compose
echo "built: $out/rnqnx_webgpu_qt_compose"
