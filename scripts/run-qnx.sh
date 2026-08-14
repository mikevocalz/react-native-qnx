#!/usr/bin/env bash
set -euo pipefail

# scripts/run-qnx.sh <arch> <app>
# Runs a deployed QNX executable on the configured target.

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

echo "Running /tmp/${APP} on ${TARGET_USER}@${QNX_TARGET_IP}..."
ssh "${TARGET_USER}@${QNX_TARGET_IP}" "/tmp/${APP}"
