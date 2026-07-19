#!/usr/bin/env bash
set -euo pipefail

# scripts/configure-qnx.sh <arch> <config>
# Configures the CMake build for the requested QNX target architecture and build type.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

ARCH="${1:-aarch64}"
CONFIG="${2:-Debug}"

if [[ "${ARCH}" != "aarch64" && "${ARCH}" != "x86_64" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <Debug|Release|RelWithDebInfo>"
  exit 1
fi

BUILD_DIR="${PROJECT_ROOT}/build/qnx-$(echo "${ARCH}" | tr '[:upper:]' '[:lower:]')-$(echo "${CONFIG}" | tr '[:upper:]' '[:lower:]')"

echo "Configuring react-native-qnx for QNX ${ARCH} ${CONFIG}..."
echo "Build directory: ${BUILD_DIR}"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

cmake \
  -DCMAKE_TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/toolchains/qnx-8.0.cmake" \
  -DQNX_TARGET_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE="${CONFIG}" \
  -G Ninja \
  -S "${PROJECT_ROOT}" \
  -B "${BUILD_DIR}"

echo ""
echo "Configure complete: ${BUILD_DIR}"
echo "Build with: ./scripts/build-qnx.sh ${ARCH} ${CONFIG}"
