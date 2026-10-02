#!/usr/bin/env bash
set -euo pipefail

# Builds and runs the current Hermes + Nitro + Expo Modules QNX integration
# probe on the physical AArch64 QSTI target.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

[[ -n "${QNX_HOST:-}" ]] || {
  echo "ERROR: QNX_HOST is not set. Source the QNX SDP 8.0.5 environment first." >&2
  exit 1
}
[[ -n "${QNX_TARGET:-}" ]] || {
  echo "ERROR: QNX_TARGET is not set. Source the QNX SDP 8.0.5 environment first." >&2
  exit 1
}

# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"
rnqnx_target_init

APP="rnqnx_runtime_qnx_probe"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
EVIDENCE_DIR="${PROJECT_ROOT}/artifacts/pi-runtime/${STAMP}"
mkdir -p "${EVIDENCE_DIR}"

echo "================================================="
echo " react-native-qnx · Hermes/Nitro/Expo target gate"
echo "================================================="
echo "Target: $(rnqnx_target_address)"
echo "Evidence: ${EVIDENCE_DIR}"
echo

echo "[1/6] Physical-target preflight"
RNQNX_PI_EVIDENCE_DIR="${EVIDENCE_DIR}/preflight"   bash "${SCRIPT_DIR}/pi-preflight.sh"

echo
echo "[2/6] Preparing exact React Native Hermes source"
bash "${SCRIPT_DIR}/prepare-hermes-qnx.sh"

echo
echo "[3/6] Configuring and building Hermes for QNX AArch64"
bash "${SCRIPT_DIR}/configure-hermes-qnx.sh" aarch64 MinSizeRel
cmake --build "${PROJECT_ROOT}/build/hermes-qnx-aarch64"   --parallel   --target hermesvmlean jsi

HERMES_SO="${PROJECT_ROOT}/build/hermes-qnx-aarch64/lib/libhermesvmlean.so"
[[ -f "${HERMES_SO}" ]] || {
  echo "ERROR: Hermes runtime not found: ${HERMES_SO}" >&2
  exit 1
}

echo
echo "[4/6] Building the combined QNX runtime probe"
cd "${PROJECT_ROOT}"
cmake --fresh --preset qnx-aarch64-debug   -DRNQNX_BUILD_HERMES_QNX=ON   -DRNQNX_BUILD_NITRO_QNX=ON   -DRNQNX_BUILD_EXPO_MODULES_QNX=ON
cmake --build --preset qnx-aarch64-debug   --parallel   --target "${APP}"

echo
echo "[5/6] Deploying probe and Hermes runtime"
bash "${SCRIPT_DIR}/deploy-qnx.sh" aarch64 "${APP}" Debug
rnqnx_scp_to "${HERMES_SO}" "${QNX_TARGET_DIR}/libhermesvmlean.so"

echo
echo "[6/6] Executing Hermes + Nitro + Expo through JSI"
set +e
RUN_OUTPUT="$(
  rnqnx_ssh "cd '${QNX_TARGET_DIR}' && LD_LIBRARY_PATH='${QNX_TARGET_DIR}':\${LD_LIBRARY_PATH:-} './${APP}'"
)"
RUN_RC=$?
set -e

printf '%s\n' "${RUN_OUTPUT}" | tee "${EVIDENCE_DIR}/runtime-output.txt"
if ((RUN_RC != 0)); then
  echo "ERROR: runtime integration probe exited with status ${RUN_RC}." >&2
  exit "${RUN_RC}"
fi

grep -Eq '^RNQNX_RUNTIME_QNX_OK hermes nitro expo arch=aarch64$'   "${EVIDENCE_DIR}/runtime-output.txt" || {
  echo "ERROR: combined Hermes/Nitro/Expo success marker was not found." >&2
  exit 1
}

printf '%s\n' "$(rnqnx_target_address)" >"${EVIDENCE_DIR}/target.txt"
date -u +%Y-%m-%dT%H:%M:%SZ >"${EVIDENCE_DIR}/completed-at.txt"
sha256sum "${EVIDENCE_DIR}/runtime-output.txt" >"${EVIDENCE_DIR}/sha256.txt"

echo
echo "Combined QNX runtime gate passed."
echo "Evidence: ${EVIDENCE_DIR}"
