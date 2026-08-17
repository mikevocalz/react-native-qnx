#!/usr/bin/env bash
set -euo pipefail

# scripts/deploy-qnx.sh <arch> <app> <config>
# Deploys the built QNX executable to a configured target.
#
# Required environment variables:
#   QNX_TARGET_IP   - target device IP or hostname
#   QNX_TARGET_USER - verified target ssh user (required)
#
# Optional environment variables:
#   QNX_SSH_PORT    - SSH port on the target (default: 22)
#   QNX_SSH_KEY     - path to a private SSH key to use
#   QNX_SSH_OPTS    - extra SSH/SCP options (default: empty)
#   QNX_TARGET_DIR  - remote directory for the binary (default: /tmp)

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
SSH_PORT="${QNX_SSH_PORT:-22}"
SSH_KEY="${QNX_SSH_KEY:-}"
SSH_OPTS="${QNX_SSH_OPTS:-}"
TARGET_DIR="${QNX_TARGET_DIR:-/tmp}"
TARGET_PATH="${TARGET_DIR}/${APP}"
BUILD_DIR="${PROJECT_ROOT}/build/qnx-$(echo "${ARCH}" | tr '[:upper:]' '[:lower:]')-$(echo "${CONFIG}" | tr '[:upper:]' '[:lower:]')"
BIN="${BUILD_DIR}/bin/${APP}"

if [[ ! -f "${BIN}" ]]; then
  echo "ERROR: Binary not found: ${BIN}"
  echo "Run ./scripts/build-qnx.sh ${ARCH} ${CONFIG} first."
  exit 1
fi

SSH_ARGS=()
SCP_ARGS=()
if [[ -n "${SSH_KEY}" ]]; then
  SSH_ARGS+=("-i" "${SSH_KEY}")
  SCP_ARGS+=("-i" "${SSH_KEY}")
fi
if [[ "${SSH_PORT}" != "22" ]]; then
  SSH_ARGS+=("-p" "${SSH_PORT}")
  SCP_ARGS+=("-P" "${SSH_PORT}")
fi
if [[ -n "${SSH_OPTS}" ]]; then
  # shellcheck disable=SC2206
  read -ra SSH_OPTS_ARR <<< "${SSH_OPTS}"
  SSH_ARGS+=("${SSH_OPTS_ARR[@]}")
  SCP_ARGS+=("${SSH_OPTS_ARR[@]}")
fi

echo "Deploying ${BIN} to ${TARGET_USER}@${QNX_TARGET_IP}:${TARGET_PATH}..."
scp "${SCP_ARGS[@]}" "${BIN}" "${TARGET_USER}@${QNX_TARGET_IP}:${TARGET_PATH}"
ssh "${SSH_ARGS[@]}" "${TARGET_USER}@${QNX_TARGET_IP}" "chmod +x ${TARGET_PATH}"

echo "Deployment complete. Run with: ./scripts/run-qnx.sh ${ARCH} ${APP}"
