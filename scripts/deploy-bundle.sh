#!/usr/bin/env bash
set -euo pipefail

# scripts/deploy-bundle.sh <local.hbc> <remote-name.hbc> [--run <runner>]
# Pushes one Hermes bytecode bundle to the configured QNX target over ssh and
# replaces it atomically. The runner executable and its Qt closure must already
# be on the target (see docs/getting-started/pi400-hello-world.md).
# Remote bundle directory: ${QNX_BUNDLE_DIR:-${QNX_TARGET_DIR}/bundles}.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"

LOCAL="${1:-}"
NAME="${2:-}"
RUNNER=""
if [[ "${3:-}" == "--run" ]]; then
  RUNNER="${4:-}"
elif [[ -n "${3:-}" ]]; then
  echo "Usage: $0 <local.hbc> <remote-name.hbc> [--run <runner>]" >&2
  exit 1
fi
if [[ -z "${LOCAL}" || -z "${NAME}" ]]; then
  echo "Usage: $0 <local.hbc> <remote-name.hbc> [--run <runner>]" >&2
  exit 1
fi
if [[ ! "${NAME}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*\.hbc$ ]]; then
  echo "ERROR: remote name must be a plain file name ending in .hbc: ${NAME}" >&2
  exit 1
fi
if [[ -n "${RUNNER}" && ! "${RUNNER}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]]; then
  echo "ERROR: runner contains unsupported characters: ${RUNNER}" >&2
  exit 1
fi
if [[ ! -s "${LOCAL}" ]]; then
  echo "ERROR: bytecode not found or empty: ${LOCAL}" >&2
  exit 1
fi

rnqnx_target_init
: "${QNX_BUNDLE_DIR:=${QNX_TARGET_DIR}/bundles}"
if [[ ! "${QNX_BUNDLE_DIR}" =~ ^/[A-Za-z0-9._/-]+$ ]]; then
  echo "ERROR: QNX_BUNDLE_DIR must be an absolute path of letters, numbers, . _ / and -." >&2
  exit 1
fi

REMOTE="${QNX_BUNDLE_DIR}/${NAME}"
REMOTE_TMP="${REMOTE}.upload.$$"
LOCAL_SHA="$(openssl dgst -sha256 "${LOCAL}" | awk '{print $NF}')"

echo "Pushing ${LOCAL} (sha256 ${LOCAL_SHA})"
echo "     -> $(rnqnx_target_address):${REMOTE}"

rnqnx_ssh "mkdir -p '${QNX_BUNDLE_DIR}' && test -w '${QNX_BUNDLE_DIR}'"
cleanup_remote_tmp() {
  rnqnx_ssh "rm -f '${REMOTE_TMP}'" >/dev/null 2>&1 || true
}
trap cleanup_remote_tmp ERR INT TERM
rnqnx_scp_to "${LOCAL}" "${REMOTE_TMP}"

# Compare hashes before the swap. The target may lack every hash tool; then the
# size is compared and the receipt says the hash was not checked on the target.
REMOTE_SHA="$(rnqnx_ssh "(sha256sum '${REMOTE_TMP}' 2>/dev/null || sha256 '${REMOTE_TMP}' 2>/dev/null || openssl dgst -sha256 '${REMOTE_TMP}' 2>/dev/null) | tail -n 1" || true)"
REMOTE_SHA="$(printf '%s' "${REMOTE_SHA}" | grep -oE '[0-9a-f]{64}' | head -n 1 || true)"
if [[ -n "${REMOTE_SHA}" ]]; then
  if [[ "${REMOTE_SHA}" != "${LOCAL_SHA}" ]]; then
    echo "ERROR: target hash ${REMOTE_SHA} differs from local ${LOCAL_SHA}; not installing." >&2
    cleanup_remote_tmp
    exit 1
  fi
  echo "Target sha256 matches."
else
  LOCAL_SIZE="$(wc -c <"${LOCAL}" | tr -d ' ')"
  REMOTE_SIZE="$(rnqnx_ssh "wc -c < '${REMOTE_TMP}'" | tr -d ' ')"
  if [[ "${REMOTE_SIZE}" != "${LOCAL_SIZE}" ]]; then
    echo "ERROR: target size ${REMOTE_SIZE} differs from local ${LOCAL_SIZE}; not installing." >&2
    cleanup_remote_tmp
    exit 1
  fi
  echo "WARNING: no hash tool on the target; only the size (${LOCAL_SIZE} bytes) was compared." >&2
fi

rnqnx_ssh "chmod 644 '${REMOTE_TMP}' && mv -f '${REMOTE_TMP}' '${REMOTE}'"
trap - ERR INT TERM
echo "Deployment complete: ${REMOTE}"

if [[ -n "${RUNNER}" ]]; then
  echo "Running ${QNX_TARGET_DIR}/${RUNNER} --bundle ${REMOTE}"
  rnqnx_ssh "cd '${QNX_TARGET_DIR}' && exec './${RUNNER}' --bundle '${REMOTE}'"
else
  echo "Run with: pnpm qnx deploy-bundle --app <name> --run, or start the runner with --bundle ${REMOTE}"
fi
