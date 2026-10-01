#!/usr/bin/env bash
set -euo pipefail

# scripts/run-qnx.sh <arch> <app>
# Runs a deployed QNX executable on the configured target.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"

ARCH="${1:-aarch64}"
APP="${2:-rnqnx_playground}"

if [[ "${ARCH}" != "aarch64" && "${ARCH}" != "x86_64" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <app>"
  exit 1
fi
if [[ ! "${APP}" =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "ERROR: app target contains unsupported characters: ${APP}" >&2
  exit 1
fi

rnqnx_target_init
echo "Running ${QNX_TARGET_DIR}/${APP} on $(rnqnx_target_address)..."
rnqnx_ssh "cd '${QNX_TARGET_DIR}' && exec './${APP}'"
