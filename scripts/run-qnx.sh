#!/usr/bin/env bash
set -euo pipefail

# scripts/run-qnx.sh <arch> <app>
# Runs a deployed QNX executable on the configured target.
#
# Required environment variables:
#   QNX_TARGET_IP   - target device IP or hostname
#   QNX_TARGET_USER - verified target ssh user (required)
#
# Optional environment variables:
#   QNX_SSH_PORT    - SSH port on the target (default: 22)
#   QNX_SSH_KEY     - path to a private SSH key to use
#   QNX_SSH_OPTS    - extra SSH options (default: empty)
#   QNX_TARGET_DIR  - remote directory for the binary (default: /tmp)

ARCH="${1:-aarch64}"
APP="${2:-rnqnx_playground}"

if [[ "${ARCH}" != "aarch64" && "${ARCH}" != "x86_64" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <app>"
  exit 1
fi

if [[ -z "${QNX_TARGET_IP:-}" ]]; then
  echo "ERROR: QNX_TARGET_IP is not set."
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

SSH_ARGS=()
if [[ -n "${SSH_KEY}" ]]; then
  SSH_ARGS+=("-i" "${SSH_KEY}")
fi
if [[ "${SSH_PORT}" != "22" ]]; then
  SSH_ARGS+=("-p" "${SSH_PORT}")
fi
if [[ -n "${SSH_OPTS}" ]]; then
  # shellcheck disable=SC2206
  read -ra SSH_OPTS_ARR <<< "${SSH_OPTS}"
  SSH_ARGS+=("${SSH_OPTS_ARR[@]}")
fi

echo "Running ${TARGET_PATH} on ${TARGET_USER}@${QNX_TARGET_IP}..."
ssh "${SSH_ARGS[@]}" "${TARGET_USER}@${QNX_TARGET_IP}" "${TARGET_PATH}"
