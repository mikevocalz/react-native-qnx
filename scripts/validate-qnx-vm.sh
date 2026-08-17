#!/usr/bin/env bash
set -euo pipefail

# scripts/validate-qnx-vm.sh
#
# Validates the local QNX x86_64 QEMU VM target described in
# docs/handoff/QNX_VM_WINDOWS.md.

# Source the QNX SDP environment if it has not already been sourced.
# Save the path to the host bash first; sourcing qnxsdp-env.sh prepends the QNX
# host bin directory to PATH, which can shadow the host ssh/scp and bash.
GIT_BASH="$(command -v bash)"
if [[ -z "${QNX_HOST:-}" ]] || [[ -z "${QNX_TARGET:-}" ]]; then
  if [[ -f /c/Users/Owner/qnx800/qnxsdp-env.sh ]]; then
    source /c/Users/Owner/qnx800/qnxsdp-env.sh
  elif [[ -f /cygdrive/c/Users/Owner/qnx800/qnxsdp-env.sh ]]; then
    source /cygdrive/c/Users/Owner/qnx800/qnxsdp-env.sh
  fi
fi
#
# Builds the G0 C++20 probe and the Screen-only Milestone 0 executable for the
# qnx-x86_64-debug preset, deploys them to the VM over SSH, runs them, and
# captures the outputs and round-trip timings in artifacts/g0-vm/.
#
# The script is intended to run from Git Bash on Windows with the VM already
# booted, but it works on any host that can reach the configured target.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EVIDENCE_DIR="${PROJECT_ROOT}/artifacts/g0-vm"

function fail() {
  echo "ERROR: $1" >&2
  exit 1
}

# Defaults for the local x86_64 QEMU VM. Override any variable by exporting it
# before invoking this script.
export QNX_TARGET_IP="${QNX_TARGET_IP:-127.0.0.1}"
export QNX_TARGET_USER="${QNX_TARGET_USER:-root}"
export QNX_SSH_PORT="${QNX_SSH_PORT:-2222}"
export QNX_TARGET_DIR="${QNX_TARGET_DIR:-/data/home/root}"

if [[ -n "${HOME:-}" ]]; then
  # Convert Git Bash /c/Users/... and /cygdrive/c/Users/... paths to the
  # POSIX form expected by OpenSSH.
  case "${HOME}" in
    /cygdrive/c/*)
      SSH_KEY_HOME="${HOME}/.ssh/id_ed25519_qnx"
      ;;
    /c/*)
      SSH_KEY_HOME="${HOME}/.ssh/id_ed25519_qnx"
      ;;
    *)
      # Assume a native Windows HOME; ssh on Windows also accepts C:\... paths.
      SSH_KEY_HOME="${HOME}/.ssh/id_ed25519_qnx"
      ;;
  esac
else
  SSH_KEY_HOME=""
fi
export QNX_SSH_KEY="${QNX_SSH_KEY:-${SSH_KEY_HOME}}"

# The Windows OpenSSH client and QNX sshd disagree on the default MAC during
# rekeying; force a common algorithm. Host-key checks are disabled for the
# throw-away local VM.
export QNX_SSH_OPTS="${QNX_SSH_OPTS:--o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o MACs=hmac-sha2-256}"

[[ -n "${QNX_SSH_KEY}" ]] && [[ -f "${QNX_SSH_KEY}" ]] ||
  fail "QNX_SSH_KEY does not name a readable file: ${QNX_SSH_KEY:-<unset>}"

for tool in cmake ssh scp; do
  command -v "${tool}" >/dev/null 2>&1 || fail "${tool} is not available on PATH."
done

mkdir -p "${EVIDENCE_DIR}"

cd "${PROJECT_ROOT}"

echo "Auditing Screen symbols used by QnxScreenSurface..."
./scripts/audit-screen-symbols.sh

echo "Configuring and building qnx-x86_64-debug..."
cmake --preset qnx-x86_64-debug
cmake --build --preset qnx-x86_64-debug --target rnqnx_g0_cxx20_probe
cmake --build --preset qnx-x86_64-debug --target rnqnx_playground

function validate_app() {
  local app="$1"
  local expected_pattern="$2"
  local evidence_file="$3"
  local roundtrip_file="$4"

  echo ""
  echo "Validating ${app}..."
  local start_epoch end_elapsed output
  start_epoch="$(date +%s)"
  QNX_TARGET_IP="${QNX_TARGET_IP}" \
  QNX_TARGET_USER="${QNX_TARGET_USER}" \
  QNX_SSH_PORT="${QNX_SSH_PORT}" \
  QNX_SSH_KEY="${QNX_SSH_KEY}" \
  QNX_SSH_OPTS="${QNX_SSH_OPTS}" \
  QNX_TARGET_DIR="${QNX_TARGET_DIR}" \
    "${GIT_BASH}" ./scripts/deploy-qnx.sh x86_64 "${app}" Debug
  output="$(
    QNX_TARGET_IP="${QNX_TARGET_IP}" \
    QNX_TARGET_USER="${QNX_TARGET_USER}" \
    QNX_SSH_PORT="${QNX_SSH_PORT}" \
    QNX_SSH_KEY="${QNX_SSH_KEY}" \
    QNX_SSH_OPTS="${QNX_SSH_OPTS}" \
    QNX_TARGET_DIR="${QNX_TARGET_DIR}" \
      "${GIT_BASH}" ./scripts/run-qnx.sh x86_64 "${app}"
  )"
  end_elapsed="$(($(date +%s) - start_epoch))"

  printf '%s\n' "${output}" >"${evidence_file}"
  cat "${evidence_file}"
  printf '%s\n' "${end_elapsed}" >"${roundtrip_file}"

  printf '%s\n' "${output}" | grep -Eq "${expected_pattern}" ||
    fail "${app} output did not match expected pattern: ${expected_pattern}"

  echo "${app} passed (${end_elapsed}s)."
}

validate_app \
  rnqnx_g0_cxx20_probe \
  "RNQNX_G0_CXX20_OK ranges=12 designated=21 (jthread=34|thread=34 jthread=unsupported)" \
  "${EVIDENCE_DIR}/cxx20-output.txt" \
  "${EVIDENCE_DIR}/cxx20-round-trip-seconds.txt"

validate_app \
  rnqnx_playground \
  "rnqnx_playground: Screen surface initialized \\(1280x720\\)" \
  "${EVIDENCE_DIR}/playground-output.txt" \
  "${EVIDENCE_DIR}/playground-round-trip-seconds.txt"

# Also capture compiler identity for the evidence bundle.
qcc -V >"${EVIDENCE_DIR}/qcc-V.txt" 2>&1 || true
q++ -V >"${EVIDENCE_DIR}/qxx-V.txt" 2>&1 || true

echo ""
echo "QNX x86_64 VM validation passed. Evidence is in ${EVIDENCE_DIR}."
