#!/usr/bin/env bash
set -euo pipefail

# scripts/configure-qnx.sh <arch> <config>
# Configures the CMake build for the requested QNX target architecture and build type.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

ARCH="${1:-aarch64}"
CONFIG="${2:-Debug}"

if [[ "${ARCH}" != "aarch64" && "${ARCH}" != "x86_64" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <Debug|Release>"
  exit 1
fi

if [[ "${CONFIG}" != "Debug" && "${CONFIG}" != "Release" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <Debug|Release>"
  exit 1
fi

PRESET="qnx-$(echo "${ARCH}" | tr '[:upper:]' '[:lower:]')-$(echo "${CONFIG}" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="${PROJECT_ROOT}/build/${PRESET}"

echo "Configuring react-native-qnx for QNX ${ARCH} ${CONFIG}..."
echo "Build directory: ${BUILD_DIR}"

cd "${PROJECT_ROOT}"
cmake --fresh --preset "${PRESET}"

echo ""
echo "Configure complete: ${BUILD_DIR}"
echo "Build with: ./scripts/build-qnx.sh ${ARCH} ${CONFIG}"
