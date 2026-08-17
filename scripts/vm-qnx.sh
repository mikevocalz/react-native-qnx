#!/usr/bin/env bash
set -euo pipefail

# scripts/vm-qnx.sh <deploy|run|deploy-run> <arch> <app> <config>
#
# Convenience wrapper for the local QNX x86_64 QEMU VM described in
# docs/handoff/QNX_VM_WINDOWS.md. The VM must already be running with the
# host SSH port forwarded to 2222 (see QNX_VM_WINDOWS.md for the QEMU
# command).
#
# This wrapper only sets the environment variables expected by
# scripts/deploy-qnx.sh and scripts/run-qnx.sh. Override any variable by
# exporting it before invoking this script.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

ACTION="${1:-deploy-run}"
ARCH="${2:-x86_64}"
APP="${3:-rnqnx_g0_cxx20_probe}"
CONFIG="${4:-Debug}"

if [[ "${ACTION}" != "deploy" && "${ACTION}" != "run" && "${ACTION}" != "deploy-run" ]]; then
  echo "Usage: $0 <deploy|run|deploy-run> <arch> <app> <config>"
  exit 1
fi

if [[ "${ARCH}" != "aarch64" && "${ARCH}" != "x86_64" ]]; then
  echo "ERROR: ARCH must be aarch64 or x86_64"
  exit 1
fi

# Defaults for the local x86_64 VM. Override by exporting before invocation.
export QNX_TARGET_IP="${QNX_TARGET_IP:-127.0.0.1}"
export QNX_TARGET_USER="${QNX_TARGET_USER:-root}"
export QNX_SSH_PORT="${QNX_SSH_PORT:-2222}"
export QNX_TARGET_DIR="${QNX_TARGET_DIR:-/data/home/root}"

SSH_KEY_HOME=""
if [[ -n "${HOME:-}" ]]; then
  # Accept both /c/Users/Owner and C:\Users\Owner forms from Git Bash.
  if [[ "${HOME}" == /c/* || "${HOME}" == /cygdrive/c/* ]]; then
    case "${HOME}" in
      /cygdrive/c/*) SSH_KEY_HOME="${HOME}/.ssh/id_ed25519_qnx" ;;
      *) SSH_KEY_HOME="${HOME}/.ssh/id_ed25519_qnx" ;;
    esac
  else
    # Native Windows HOME from PowerShell/CMD: translate to POSIX for ssh.
    SSH_KEY_HOME="$(cygpath -u "${HOME}" 2>/dev/null)/.ssh/id_ed25519_qnx" || true
  fi
fi
export QNX_SSH_KEY="${QNX_SSH_KEY:-${SSH_KEY_HOME}}"

# The Windows OpenSSH client and QNX sshd disagree on the default MAC during
# rekeying; force a common algorithm. Also disable host-key checks for the
# throw-away local VM.
export QNX_SSH_OPTS="${QNX_SSH_OPTS:--o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o MACs=hmac-sha2-256}"

case "${ACTION}" in
  deploy)
    "${SCRIPT_DIR}/deploy-qnx.sh" "${ARCH}" "${APP}" "${CONFIG}"
    ;;
  run)
    "${SCRIPT_DIR}/run-qnx.sh" "${ARCH}" "${APP}"
    ;;
  deploy-run)
    "${SCRIPT_DIR}/deploy-qnx.sh" "${ARCH}" "${APP}" "${CONFIG}"
    "${SCRIPT_DIR}/run-qnx.sh" "${ARCH}" "${APP}"
    ;;
esac
