#!/usr/bin/env bash
set -euo pipefail

# scripts/log-qnx.sh
# Streams logs from the target QNX device.

if [[ -z "${QNX_TARGET_IP:-}" ]]; then
  echo "ERROR: QNX_TARGET_IP is not set."
  exit 1
fi

USER="${QNX_TARGET_USER:-root}"

echo "Streaming slog2info from ${USER}@${QNX_TARGET_IP}..."
ssh "${USER}@${QNX_TARGET_IP}" "slog2info -w" || true
