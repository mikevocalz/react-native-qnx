#!/usr/bin/env bash
set -euo pipefail

# scripts/build-qnx.sh <arch> <config>
# Builds the configured QNX target.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

ARCH="${1:-aarch64}"
CONFIG="${2:-Debug}"

echo "Building react-native-qnx for QNX ${ARCH} ${CONFIG}..."

BUILD_DIR="${PROJECT_ROOT}/build/qnx-$(echo "${ARCH}" | tr '[:upper:]' '[:lower:]')-$(echo "${CONFIG}" | tr '[:upper:]' '[:lower:]')"

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "Build directory not found. Run ./scripts/configure-qnx.sh ${ARCH} ${CONFIG} first."
  exit 1
fi

cmake --build "${BUILD_DIR}" --parallel

echo ""
echo "Build complete. Artifacts in ${BUILD_DIR}/bin"
