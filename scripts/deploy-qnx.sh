#!/usr/bin/env bash
set -euo pipefail

# scripts/deploy-qnx.sh <arch> <app> <config> [--rollback]
# Atomically deploys a built QNX executable to the configured target.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"

ARCH="${1:-aarch64}"
APP="${2:-rnqnx_playground}"
CONFIG="${3:-Debug}"
MODE="${4:-deploy}"
if [[ $# -gt 4 || ( "${MODE}" != "deploy" && "${MODE}" != "--rollback" ) ]]; then
  echo "Usage: $0 <aarch64|x86_64> <app> <Debug|Release> [--rollback]" >&2
  exit 1
fi

if [[ "${ARCH}" != "aarch64" && "${ARCH}" != "x86_64" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <app> <Debug|Release>"
  exit 1
fi
if [[ "${CONFIG}" != "Debug" && "${CONFIG}" != "Release" ]]; then
  echo "Usage: $0 <aarch64|x86_64> <app> <Debug|Release>"
  exit 1
fi
if [[ ! "${APP}" =~ ^[A-Za-z0-9_][A-Za-z0-9._-]*$ || "${APP}" == "." || "${APP}" == ".." ]]; then
  echo "ERROR: app target contains unsupported characters: ${APP}" >&2
  exit 1
fi

rnqnx_target_init

BUILD_DIR="${PROJECT_ROOT}/build/qnx-$(printf '%s' "${ARCH}" | tr '[:upper:]' '[:lower:]')-$(printf '%s' "${CONFIG}" | tr '[:upper:]' '[:lower:]')"
BIN="${BUILD_DIR}/bin/${APP}"
TARGET_PATH="${QNX_TARGET_DIR}/${APP}"
STATE_ROOT="${QNX_TARGET_DIR}/.rnqnx-deploy"
APP_STATE="${STATE_ROOT}/${APP}"
LOCK_DIR="${APP_STATE}/lock"
REMOTE_TMP="${LOCK_DIR}/candidate"
PREVIOUS="${APP_STATE}/previous"
LOCAL_VERIFY=""
LOCK_HELD=0

if [[ "${MODE}" == "deploy" && ( ! -f "${BIN}" || -L "${BIN}" ) ]]; then
  echo "ERROR: Regular binary not found: ${BIN}" >&2
  echo "Run ./scripts/build-qnx.sh ${ARCH} ${CONFIG} first." >&2
  exit 1
fi

cleanup() {
  local status=$?
  trap - EXIT
  if [[ -n "${LOCAL_VERIFY}" ]]; then rm -f "${LOCAL_VERIFY}"; fi
  if [[ "${LOCK_HELD}" == 1 ]]; then
    rnqnx_ssh "rm -f '${REMOTE_TMP}' '${LOCK_DIR}/previous' && rmdir '${LOCK_DIR}'" >/dev/null 2>&1 || true
  fi
  exit "${status}"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# A per-app exclusive lock prevents two deployments/rollbacks sharing staging.
# Never follow a pre-existing target or backup symlink.
rnqnx_ssh "mkdir -p '${QNX_TARGET_DIR}' && test -w '${QNX_TARGET_DIR}' && test ! -L '${STATE_ROOT}' && mkdir -p '${STATE_ROOT}' && test ! -L '${APP_STATE}' && mkdir -p '${APP_STATE}' && mkdir '${LOCK_DIR}'"
LOCK_HELD=1
rnqnx_ssh "test ! -L '${TARGET_PATH}' && test ! -L '${PREVIOUS}' && { test ! -e '${TARGET_PATH}' || test -f '${TARGET_PATH}'; } && { test ! -e '${PREVIOUS}' || test -f '${PREVIOUS}'; }"

if [[ "${MODE}" == "--rollback" ]]; then
  echo "Restoring previous binary at $(rnqnx_target_address):${TARGET_PATH}"
  rnqnx_ssh "test -f '${PREVIOUS}' && cp '${PREVIOUS}' '${REMOTE_TMP}' && chmod 755 '${REMOTE_TMP}' && mv -f '${REMOTE_TMP}' '${TARGET_PATH}'"
else
  echo "Deploying ${BIN} -> $(rnqnx_target_address):${TARGET_PATH}"
  rnqnx_scp_to "${BIN}" "${REMOTE_TMP}"
  # Verify actual uploaded bytes without assuming a target SHA utility exists.
  # SSH protects transport; this round trip detects truncation/copy corruption.
  LOCAL_VERIFY="$(mktemp "${TMPDIR:-/tmp}/rnqnx-deploy.XXXXXXXX")"
  rnqnx_scp_from "${REMOTE_TMP}" "${LOCAL_VERIFY}"
  if ! cmp -s "${BIN}" "${LOCAL_VERIFY}"; then
    echo "ERROR: Uploaded binary differs; existing deployment is unchanged." >&2
    exit 1
  fi
  # Stage the backup before replacing it. Never remove the running path first.
  rnqnx_ssh "chmod 755 '${REMOTE_TMP}' && if test -f '${TARGET_PATH}'; then cp '${TARGET_PATH}' '${LOCK_DIR}/previous' && mv -f '${LOCK_DIR}/previous' '${PREVIOUS}'; fi && mv -f '${REMOTE_TMP}' '${TARGET_PATH}'"
fi

echo "Deployment complete (binary only; no process was started or stopped)."
echo "Run with: ./scripts/run-qnx.sh ${ARCH} ${APP}"
echo "Rollback with: ./scripts/deploy-qnx.sh ${ARCH} ${APP} ${CONFIG} --rollback"
