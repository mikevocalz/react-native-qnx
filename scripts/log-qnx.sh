#!/usr/bin/env bash
set -euo pipefail

# scripts/log-qnx.sh
# Streams logs from the target QNX device.

if [[ -z "${QNX_TARGET_IP:-}" ]]; then
  echo "ERROR: QNX_TARGET_IP is not set."
  exit 1
fi

if [[ -z "${QNX_TARGET_USER:-}" ]]; then
  echo "ERROR: QNX_TARGET_USER is not set. Use the verified account for this QSTI image."
  exit 1
fi

TARGET_USER="${QNX_TARGET_USER}"

echo "Streaming slog2info from ${TARGET_USER}@${QNX_TARGET_IP}..."
ssh "${TARGET_USER}@${QNX_TARGET_IP}" "slog2info -w" || true
