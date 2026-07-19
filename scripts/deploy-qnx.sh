#!/usr/bin/env bash
set -euo pipefail

# scripts/deploy-qnx.sh <arch> <app>
# Deploys the built QNX executable to a configured target.
#
# Required environment variables:
#   QNX_TARGET_IP   - target device IP or hostname
#   QNX_TARGET_USER   - target ssh user (defaults to root)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

ARCH="${1:-aarch64}"
APP="${2:-rnqnx_playground}"

if [[ -z "${QNX_TARGET_IP:-}" ]]; then
  echo "ERROR: QNX_TARGET_IP is not set. Set it to the target device IP or hostname."
  exit 1
fi

USER="${QNX_TARGET_USER:-root}"
CONFIG="Debug"
BUILD_DIR="${PROJECT_ROOT}/build/qnx-$(echo "${ARCH}" | tr '[:upper:]' '[:lower:]')-$(echo "${CONFIG}" | tr '[:upper:]' '[:lower:]')"
BIN="${BUILD_DIR}/bin/${APP}"

if [[ ! -f "${BIN}" ]]; then
  echo "ERROR: Binary not found: ${BIN}"
  echo "Run ./scripts/build-qnx.sh ${ARCH} ${CONFIG} first."
  exit 1
fi

echo "Deploying ${BIN} to ${USER}@${QNX_TARGET_IP}:/tmp/${APP}..."
scp "${BIN}" "${USER}@${QNX_TARGET_IP}:/tmp/${APP}"
ssh "${USER}@${QNX_TARGET_IP}" "chmod +x /tmp/${APP}"

echo "Deployment complete. Run with: ./scripts/run-qnx.sh ${ARCH} ${APP}"
