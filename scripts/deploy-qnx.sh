#!/usr/bin/env bash
set -euo pipefail

# scripts/deploy-qnx.sh <arch> <app> <config>
# Atomically deploys a built QNX executable to the configured target.

SCRIPT_DIR="$(cd "$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "\${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/qnx-target.sh
source "\${SCRIPT_DIR}/lib/qnx-target.sh"

ARCH="\${1:-aarch64}"
APP="\${2:-rnqnx_playground}"
CONFIG="\${3:-Debug}"

if [[ "\${ARCH}" != "aarch64" && "\${ARCH}" != "x86_64" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <app> <Debug|Release>"
  exit 1
fi
if [[ "\${CONFIG}" != "Debug" && "\${CONFIG}" != "Release" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <app> <Debug|Release>"
  exit 1
fi
if [[ ! "\${APP}" =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "ERROR: app target contains unsupported characters: \${APP}" >&2
  exit 1
fi

rnqnx_target_init

BUILD_DIR="\${PROJECT_ROOT}/build/qnx-$(printf '%s' "\${ARCH}" | tr '[:upper:]' '[:lower:]')-$(printf '%s' "\${CONFIG}" | tr '[:upper:]' '[:lower:]')"
BIN="\${BUILD_DIR}/bin/\${APP}"
TARGET_PATH="\${QNX_TARGET_DIR}/\${APP}"
REMOTE_TMP="\${TARGET_PATH}.upload.$$"

if [[ ! -f "\${BIN}" ]]; then
  echo "ERROR: Binary not found: \${BIN}" >&2
  echo "Run ./scripts/build-qnx.sh \${ARCH} \${CONFIG} first." >&2
  exit 1
fi

echo "Deploying \${BIN}"
echo "       -> $(rnqnx_target_address):\${TARGET_PATH}"

rnqnx_ssh "mkdir -p '\${QNX_TARGET_DIR}' && test -w '\${QNX_TARGET_DIR}'"

cleanup_remote_tmp() {
  rnqnx_ssh "rm -f '\${REMOTE_TMP}'" >/dev/null 2>&1 || true
}
trap cleanup_remote_tmp ERR INT TERM

rnqnx_scp_to "\${BIN}" "\${REMOTE_TMP}"
rnqnx_ssh "chmod 755 '\${REMOTE_TMP}' && mv -f '\${REMOTE_TMP}' '\${TARGET_PATH}'"

trap - ERR INT TERM

echo "Deployment complete."
echo "Run with: ./scripts/run-qnx.sh \${ARCH} \${APP}"
