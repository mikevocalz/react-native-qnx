#!/usr/bin/env bash
set -euo pipefail

# scripts/build-qnx.sh <arch> <config>
# Builds the configured QNX target.

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

echo "Building react-native-qnx for QNX ${ARCH} ${CONFIG}..."

PRESET="qnx-$(echo "${ARCH}" | tr '[:upper:]' '[:lower:]')-$(echo "${CONFIG}" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="${PROJECT_ROOT}/build/${PRESET}"

if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
  echo "Configured Ninja build not found. Run ./scripts/configure-qnx.sh ${ARCH} ${CONFIG} first."
  exit 1
fi

cd "${PROJECT_ROOT}"
cmake --build --preset "${PRESET}" --parallel

echo ""
echo "Build complete. Artifacts in ${BUILD_DIR}/bin"
