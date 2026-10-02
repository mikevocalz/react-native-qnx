#!/usr/bin/env bash
set -euo pipefail

# One-command Raspberry Pi 4/5 QSTI lane:
# host/QNX preflight -> AArch64 build -> atomic deploy -> on-glass run
# -> stdout/slog2/screenshot evidence.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

source_qnx_environment() {
  if [[ -n "${QNX_HOST:-}" && -n "${QNX_TARGET:-}" ]]; then
    return
  fi

  local candidates=()
  if [[ -n "${QNX_SDP_ENV:-}" ]]; then
    candidates+=("${QNX_SDP_ENV}")
  fi
  candidates+=(
    "${HOME}/qnx800/qnxsdp-env.sh"
    "${HOME}/qnx80x/qnxsdp-env.sh"
  )

  if [[ -n "${USERNAME:-}" ]]; then
    candidates+=(
      "/c/Users/${USERNAME}/qnx800/qnxsdp-env.sh"
      "/cygdrive/c/Users/${USERNAME}/qnx800/qnxsdp-env.sh"
    )
  fi

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -f "${candidate}" ]]; then
      echo "Sourcing QNX SDP environment: ${candidate}"
      # shellcheck disable=SC1090
      source "${candidate}"
      return
    fi
  done

  echo "ERROR: QNX SDP environment is not active." >&2
  echo "Source qnxsdp-env.sh first, or set QNX_SDP_ENV to its full path." >&2
  exit 1
}

source_qnx_environment

# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"
rnqnx_target_init

CONFIG="${RNQNX_PI_CONFIG:-Release}"
APP="rnqnx_playground"
RUNS="${RNQNX_PI_RUNS:-3}"

if [[ "${CONFIG}" != "Release" && "${CONFIG}" != "Debug" ]]; then
  echo "ERROR: RNQNX_PI_CONFIG must be Release or Debug." >&2
  exit 1
fi
if [[ ! "${RUNS}" =~ ^[0-9]+$ ]] || ((10#${RUNS} < 1 || 10#${RUNS} > 20)); then
  echo "ERROR: RNQNX_PI_RUNS must be an integer from 1 through 20." >&2
  exit 1
fi

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
EVIDENCE_DIR="${PROJECT_ROOT}/artifacts/pi/${STAMP}"
mkdir -p "${EVIDENCE_DIR}"
export RNQNX_PI_EVIDENCE_DIR="${EVIDENCE_DIR}/preflight"

echo "========================================"
echo " react-native-qnx · Raspberry Pi install"
echo "========================================"
echo "Target: $(rnqnx_target_address)"
echo "Config: ${CONFIG}"
echo "Validation runs: ${RUNS}"
echo "Remote dir: ${QNX_TARGET_DIR}"
echo "Evidence: ${EVIDENCE_DIR}"
echo

if [[ "${RNQNX_SKIP_PREFLIGHT:-0}" != "1" ]]; then
  bash "${SCRIPT_DIR}/pi-preflight.sh"
fi

if [[ "${RNQNX_SKIP_BUILD:-0}" != "1" ]]; then
  echo
  echo "[1/4] Configuring QNX AArch64 ${CONFIG}"
  bash "${SCRIPT_DIR}/configure-qnx.sh" aarch64 "${CONFIG}"

  echo
  echo "[2/4] Building ${APP}"
  PRESET="qnx-aarch64-$(printf '%s' "${CONFIG}" | tr '[:upper:]' '[:lower:]')"
  cmake --build --preset "${PRESET}" --target "${APP}" --parallel
else
  echo "[1/4] Build skipped by RNQNX_SKIP_BUILD=1"
fi

echo
echo "[3/4] Deploying atomically"
bash "${SCRIPT_DIR}/deploy-qnx.sh" aarch64 "${APP}" "${CONFIG}"

echo
echo "[4/4] Running on QNX Screen and collecting evidence"
set +e
RUN_OUTPUT="$(
  rnqnx_ssh "sh -s -- '${QNX_TARGET_DIR}' '${APP}' '${RUNS}'" <<'RNQNX_REMOTE'
target_dir="$1"
app="$2"
runs="$3"
cd "$target_dir" || exit 30
screenshot_file="${app}.bmp"
rm -f "${app}".run-*.log "$screenshot_file"

run=1
while [ "$run" -le "$runs" ]; do
  log_file="${app}.run-${run}.log"
  echo "RNQNX_RUN_BEGIN=${run}/${runs}"
  "./$app" >"$log_file" 2>&1 &
  pid=$!

  if [ "$run" -eq 1 ]; then
    sleep 2
    if command -v screenshot >/dev/null 2>&1; then
      screenshot -file="$screenshot_file" >/dev/null 2>&1 || true
    fi
  fi

  wait "$pid"
  rc=$?
  cat "$log_file"
  echo "RNQNX_RUN_END=${run}/${runs} rc=${rc}"
  if [ "$rc" -ne 0 ]; then
    exit "$rc"
  fi
  run=$((run + 1))
done
RNQNX_REMOTE
)"
RUN_RC=$?
set -e

printf '%s\n' "${RUN_OUTPUT}" | tee "${EVIDENCE_DIR}/playground-output.txt"

if ((RUN_RC != 0)); then
  echo "ERROR: target app exited with status ${RUN_RC}." >&2
  exit "${RUN_RC}"
fi

SCREEN_INIT_COUNT="$(grep -Ec 'rnqnx_playground: Screen surface initialized \([0-9]+x[0-9]+\)' "${EVIDENCE_DIR}/playground-output.txt" || true)"
CLEAN_EXIT_COUNT="$(grep -Ec 'rnqnx_playground: exiting cleanly after [0-9]+ frames' "${EVIDENCE_DIR}/playground-output.txt" || true)"
if ((10#${SCREEN_INIT_COUNT} != 10#${RUNS})); then
  echo "ERROR: expected ${RUNS} Screen initialization proof lines, found ${SCREEN_INIT_COUNT}." >&2
  exit 1
fi
if ((10#${CLEAN_EXIT_COUNT} != 10#${RUNS})); then
  echo "ERROR: expected ${RUNS} clean shutdown proof lines, found ${CLEAN_EXIT_COUNT}." >&2
  exit 1
fi

REMOTE_SCREENSHOT="${QNX_TARGET_DIR}/${APP}.bmp"
if rnqnx_ssh "test -f '${REMOTE_SCREENSHOT}'"; then
  rnqnx_scp_from "${REMOTE_SCREENSHOT}" "${EVIDENCE_DIR}/rnqnx-playground.bmp"
  echo "Captured on-glass screenshot: ${EVIDENCE_DIR}/rnqnx-playground.bmp"
else
  echo "WARNING: no screenshot was captured by the target."
fi

set +e
rnqnx_ssh "if command -v slog2info >/dev/null 2>&1; then slog2info 2>/dev/null | tail -n 250; fi" >"${EVIDENCE_DIR}/slog2-tail.txt"
SLOG_RC=$?
set -e
if ((SLOG_RC != 0)); then
  echo "WARNING: target slog2 snapshot could not be collected."
fi

printf '%s\n' "$(rnqnx_target_address)" >"${EVIDENCE_DIR}/target.txt"
printf '%s\n' "${CONFIG}" >"${EVIDENCE_DIR}/config.txt"
printf '%s\n' "${RUNS}" >"${EVIDENCE_DIR}/run-count.txt"
date -u +%Y-%m-%dT%H:%M:%SZ >"${EVIDENCE_DIR}/completed-at.txt"

echo
echo "Verifying and hashing evidence..."
node "${PROJECT_ROOT}/scripts/verify-pi-evidence.mjs" "${EVIDENCE_DIR}"

echo
echo "Raspberry Pi install smoke pass complete."
echo "Binary: ${QNX_TARGET_DIR}/${APP}"
echo "Evidence: ${EVIDENCE_DIR}"
echo "Repeated clean runs: ${RUNS}"
echo "Manifest: ${EVIDENCE_DIR}/manifest.json"
echo
echo "This proves the native QNX Screen deployment lane. The full React Native +"
echo "Hermes + Qt Quick renderer remains a separate target-integration milestone."
