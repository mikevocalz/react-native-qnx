#!/usr/bin/env bash
set -euo pipefail

# Cross-builds the specification's C++20 probe and runs that exact binary on a Pi.
# A local/host run is never reported as G0-A evidence.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EVIDENCE_DIR="${PROJECT_ROOT}/artifacts/g0"
EXPECTED_PATTERN="RNQNX_G0_CXX20_OK ranges=12 designated=21 (jthread=34|thread=34 jthread=unsupported)"
REMOTE_BINARY="/tmp/rnqnx-g0-cxx20-probe"

function fail() {
  echo "ERROR: $1" >&2
  exit 1
}

[[ "$(uname -s)" == "Linux" ]] ||
  fail "G0-A cross-validation must run on the licensed Linux QNX development host."
[[ "$(uname -m)" == "x86_64" ]] ||
  fail "G0-A cross-validation requires the canonical x86-64 development host."

for variable in QNX_HOST QNX_TARGET QNX_TARGET_IP QNX_TARGET_USER; do
  [[ -n "${!variable:-}" ]] || fail "${variable} is not set."
done

[[ "${QNX_TARGET_IP}" =~ ^[A-Za-z0-9.-]+$ ]] ||
  fail "QNX_TARGET_IP must be an IPv4 address or DNS/mDNS hostname."
[[ "${QNX_TARGET_USER}" =~ ^[A-Za-z_][A-Za-z0-9._-]*$ ]] ||
  fail "QNX_TARGET_USER contains unsupported characters."

for command_name in cmake ninja qcc q++ ssh scp; do
  command -v "${command_name}" >/dev/null 2>&1 ||
    fail "${command_name} is not available on PATH."
done

SSH_OPTIONS=(-o BatchMode=yes -o ConnectTimeout=10)
if [[ -n "${QNX_SSH_KEY:-}" ]]; then
  [[ -f "${QNX_SSH_KEY}" ]] || fail "QNX_SSH_KEY does not name a readable file."
  SSH_OPTIONS+=(-i "${QNX_SSH_KEY}")
fi

mkdir -p "${EVIDENCE_DIR}"

echo "Capturing installed QNX compiler targets..."
qcc -V >"${EVIDENCE_DIR}/qcc-V.txt" 2>&1
q++ -V >"${EVIDENCE_DIR}/qxx-V.txt" 2>&1
cat "${EVIDENCE_DIR}/qcc-V.txt"
cat "${EVIDENCE_DIR}/qxx-V.txt"

grep -Fq "gcc_ntoaarch64le" "${EVIDENCE_DIR}/qcc-V.txt" ||
  fail "Installed qcc -V output does not contain gcc_ntoaarch64le."

cd "${PROJECT_ROOT}"
cmake --fresh --preset qnx-pi4
cmake --build --preset qnx-pi4

LOCAL_BINARY="${PROJECT_ROOT}/build/qnx-pi4/bin/rnqnx_g0_cxx20_probe"
[[ -x "${LOCAL_BINARY}" ]] || fail "Cross-built probe is missing: ${LOCAL_BINARY}"

SSH_TARGET="${QNX_TARGET_USER}@${QNX_TARGET_IP}"
START_EPOCH="$(date +%s)"

scp "${SSH_OPTIONS[@]}" "${LOCAL_BINARY}" "${SSH_TARGET}:${REMOTE_BINARY}"
ssh "${SSH_OPTIONS[@]}" "${SSH_TARGET}" "chmod 755 ${REMOTE_BINARY}"
PROBE_OUTPUT="$(ssh "${SSH_OPTIONS[@]}" "${SSH_TARGET}" "${REMOTE_BINARY}")"

END_EPOCH="$(date +%s)"
ROUND_TRIP_SECONDS=$((END_EPOCH - START_EPOCH))

printf '%s\n' "${PROBE_OUTPUT}" | tee "${EVIDENCE_DIR}/pi-cxx20-output.txt"
printf '%s\n' "${ROUND_TRIP_SECONDS}" >"${EVIDENCE_DIR}/pi-round-trip-seconds.txt"

printf '%s\n' "${PROBE_OUTPUT}" | grep -Eq "${EXPECTED_PATTERN}" ||
  fail "Pi output did not contain the expected C++20 proof line."
((ROUND_TRIP_SECONDS < 30)) ||
  fail "Pi deploy-and-run round trip was ${ROUND_TRIP_SECONDS}s; G0-D requires under 30s."

echo "G0-A target proof passed; deploy-and-run round trip: ${ROUND_TRIP_SECONDS}s."
echo "Review and sanitize artifacts/g0 before recording evidence in docs/toolchain.md."
