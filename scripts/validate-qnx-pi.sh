#!/usr/bin/env bash
set -euo pipefail

# scripts/validate-qnx-pi.sh
#
# Validates the Screen-only Milestone 0 executable (rnqnx_playground) on a
# physical QNX AArch64le target such as a Raspberry Pi 4 running a QSTI image.
#
# The caller must provide the target's IP/hostname and SSH user via environment
# variables. The script cross-builds rnqnx_playground for qnx-aarch64-debug,
# deploys it, runs it, and captures the output plus round-trip timing in
# artifacts/g0-pi/.

# Source the QNX SDP environment if it has not already been sourced.
if [[ -z "${QNX_HOST:-}" ]] || [[ -z "${QNX_TARGET:-}" ]]; then
  if [[ -f /c/Users/Owner/qnx800/qnxsdp-env.sh ]]; then
    source /c/Users/Owner/qnx800/qnxsdp-env.sh
  elif [[ -f /cygdrive/c/Users/Owner/qnx800/qnxsdp-env.sh ]]; then
    source /cygdrive/c/Users/Owner/qnx800/qnxsdp-env.sh
  fi
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EVIDENCE_DIR="${PROJECT_ROOT}/artifacts/g0-pi"

function fail() {
  echo "ERROR: $1" >&2
  exit 1
}

# Target must be supplied by the caller. There are no safe defaults for a
# physical board.
[[ -n "${QNX_TARGET_IP:-}" ]] || fail "QNX_TARGET_IP is not set. Set it to the Pi/QSTI target IP or hostname."
[[ -n "${QNX_TARGET_USER:-}" ]] || fail "QNX_TARGET_USER is not set. Set it to the verified SSH user on the target."

# Use the standard SSH port 22 unless the board is reached through a different
# forwarded port.
export QNX_TARGET_IP="${QNX_TARGET_IP}"
export QNX_TARGET_USER="${QNX_TARGET_USER}"
export QNX_SSH_PORT="${QNX_SSH_PORT:-22}"
export QNX_TARGET_DIR="${QNX_TARGET_DIR:-/data/home/${QNX_TARGET_USER}}"
export QNX_SSH_OPTS="${QNX_SSH_OPTS:-}"

if [[ -n "${QNX_SSH_KEY:-}" ]]; then
  [[ -f "${QNX_SSH_KEY}" ]] || fail "QNX_SSH_KEY does not name a readable file: ${QNX_SSH_KEY}"
fi

for tool in cmake ssh scp; do
  command -v "${tool}" >/dev/null 2>&1 || fail "${tool} is not available on PATH."
done

mkdir -p "${EVIDENCE_DIR}"

cd "${PROJECT_ROOT}"

echo "Configuring and building qnx-aarch64-debug..."
cmake --preset qnx-aarch64-debug
cmake --build --preset qnx-aarch64-debug --target rnqnx_playground

LOCAL_BINARY="${PROJECT_ROOT}/build/qnx-aarch64-debug/bin/rnqnx_playground"
[[ -x "${LOCAL_BINARY}" ]] || fail "Cross-built binary is missing: ${LOCAL_BINARY}"

# Verify the binary is actually AArch64le.
READELF=""
for candidate in ntoaarch64-readelf aarch64-unknown-nto-qnx8.0.0-readelf; do
  if command -v "${candidate}" >/dev/null 2>&1; then
    READELF="${candidate}"
    break
  fi
done
if [[ -n "${READELF}" ]]; then
  MACHINE="$(${READELF} -h "${LOCAL_BINARY}" | awk '/Machine:/ {print $NF}')"
  echo "Binary machine type: ${MACHINE}"
  [[ "${MACHINE}" == "AArch64" ]] || fail "Binary is not AArch64: ${MACHINE}"
  ${READELF} -h "${LOCAL_BINARY}" >"${EVIDENCE_DIR}/rnqnx_playground-readelf.txt"
else
  echo "WARNING: no AArch64 readelf found; skipping machine-type verification."
fi

echo ""
echo "Validating rnqnx_playground on ${QNX_TARGET_USER}@${QNX_TARGET_IP}..."
START_EPOCH="$(date +%s)"

# Save the host bash path before sourcing qnxsdp-env.sh shadows it.
GIT_BASH="$(command -v bash)"

./scripts/deploy-qnx.sh aarch64 rnqnx_playground Debug
OUTPUT="$("${GIT_BASH}" ./scripts/run-qnx.sh aarch64 rnqnx_playground)"

END_EPOCH="$(date +%s)"
ROUND_TRIP_SECONDS=$((END_EPOCH - START_EPOCH))

printf '%s\n' "${OUTPUT}" >"${EVIDENCE_DIR}/playground-output.txt"
cat "${EVIDENCE_DIR}/playground-output.txt"
printf '%s\n' "${ROUND_TRIP_SECONDS}" >"${EVIDENCE_DIR}/playground-round-trip-seconds.txt"

printf '%s\n' "${OUTPUT}" | grep -Eq 'rnqnx_playground: Screen surface initialized \\(1280x720\\)' ||
  fail "rnqnx_playground did not initialize a 1280x720 Screen surface on the target."

printf '%s\n' "${OUTPUT}" | grep -Eq 'rnqnx_playground: exiting cleanly after [0-9]+ frames' ||
  fail "rnqnx_playground did not exit cleanly on the target."

echo "rnqnx_playground passed on ${QNX_TARGET_IP} (${ROUND_TRIP_SECONDS}s)."
echo "Evidence is in ${EVIDENCE_DIR}."
