#!/usr/bin/env bash
set -euo pipefail

# scripts/run-qnx.sh <arch> <app>
# Runs a deployed QNX executable on the configured target.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

ARCH="${1:-aarch64}"
APP="${2:-rnqnx_playground}"

if [[ -z "${QNX_TARGET_IP:-}" ]]; then
  echo "ERROR: QNX_TARGET_IP is not set."
  exit 1
fi

USER="${QNX_TARGET_USER:-root}"

echo "Running /tmp/${APP} on ${USER}@${QNX_TARGET_IP}..."
ssh "${USER}@${QNX_TARGET_IP}" "/tmp/${APP}" || true
