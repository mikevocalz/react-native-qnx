#!/usr/bin/env bash
set -euo pipefail

# scripts/deploy-qnx.sh <arch> <app> <config>
# Deploys the built QNX executable to a configured target.
#
# Required environment variables:
#   QNX_TARGET_IP   - target device IP or hostname
#   QNX_TARGET_USER   - verified target ssh user (required)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

ARCH="${1:-aarch64}"
APP="${2:-rnqnx_playground}"
CONFIG="${3:-Debug}"

if [[ "${ARCH}" != "aarch64" && "${ARCH}" != "x86_64" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <app> <Debug|Release>"
  exit 1
fi

if [[ "${CONFIG}" != "Debug" && "${CONFIG}" != "Release" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <app> <Debug|Release>"
  exit 1
fi

if [[ -z "${QNX_TARGET_IP:-}" ]]; then
  echo "ERROR: QNX_TARGET_IP is not set. Set it to the target device IP or hostname."
  exit 1
fi

if [[ -z "${QNX_TARGET_USER:-}" ]]; then
  echo "ERROR: QNX_TARGET_USER is not set. Use the verified account for this QSTI image."
  exit 1
fi

TARGET_USER="${QNX_TARGET_USER}"
BUILD_DIR="${PROJECT_ROOT}/build/qnx-$(echo "${ARCH}" | tr '[:upper:]' '[:lower:]')-$(echo "${CONFIG}" | tr '[:upper:]' '[:lower:]')"
BIN="${BUILD_DIR}/bin/${APP}"

if [[ ! -f "${BIN}" ]]; then
  echo "ERROR: Binary not found: ${BIN}"
  echo "Run ./scripts/build-qnx.sh ${ARCH} ${CONFIG} first."
  exit 1
fi

echo "Deploying ${BIN} to ${TARGET_USER}@${QNX_TARGET_IP}:/tmp/${APP}..."
scp "${BIN}" "${TARGET_USER}@${QNX_TARGET_IP}:/tmp/${APP}"
ssh "${TARGET_USER}@${QNX_TARGET_IP}" "chmod +x /tmp/${APP}"

echo "Deployment complete. Run with: ./scripts/run-qnx.sh ${ARCH} ${APP}"
