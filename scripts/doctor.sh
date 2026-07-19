#!/usr/bin/env bash
set -euo pipefail

# scripts/doctor.sh
# Validates the development environment for react-native-qnx.
# Exit code 0 means the environment is ready to configure/build for QNX.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LOG_DIR="${PROJECT_ROOT}/artifacts/logs"
LOG_FILE="${LOG_DIR}/$(date +%Y-%m-%d)-qnx-doctor.txt"

mkdir -p "${LOG_DIR}"

echo "react-native-qnx doctor" | tee "${LOG_FILE}"
echo "Started at $(date -u +%Y-%m-%dT%H:%M:%SZ)" | tee -a "${LOG_FILE}"
echo "Log: ${LOG_FILE}"
echo ""

ERRORS=0
WARNINGS=0

function fail() {
  echo "  ERROR: $1" | tee -a "${LOG_FILE}"
  ERRORS=$((ERRORS + 1))
}

function warn() {
  echo "  WARNING: $1" | tee -a "${LOG_FILE}"
  WARNINGS=$((WARNINGS + 1))
}

function ok() {
  echo "  OK: $1" | tee -a "${LOG_FILE}"
}

function check_cmd() {
  local name="$1"
  local min_version="${2:-}"
  if ! command -v "${name}" >/dev/null 2>&1; then
    fail "${name} is not installed or not in PATH"
    return
  fi

  local version_output
  local exit_code
  set +e
  version_output="$(${name} --version 2>&1)"
  exit_code=$?
  set -e

  if [[ ${exit_code} -ne 0 ]]; then
    fail "${name} --version failed: ${version_output}"
    return
  fi

  local version
  version="$(echo "${version_output}" | head -n1)"
  if [[ -n "${min_version}" ]]; then
    ok "${name} found (${version})"
  else
    ok "${name} found (${version})"
  fi
}

function require_env() {
  local var="$1"
  local message="$2"
  if [[ -z "${!var:-}" ]]; then
    fail "Environment variable ${var} is not set. ${message}"
  else
    ok "${var}=${!var}"
  fi
}

echo "[1/8] Host environment" | tee -a "${LOG_FILE}"
if [[ "$(uname -s)" == "Linux" ]]; then
  ok "Host OS is Linux ($(uname -srm))"
else
  warn "Host OS is $(uname -s). Primary build host should be x86-64 Ubuntu 24.04."
fi

if [[ "$(uname -m)" == "x86_64" ]]; then
  ok "Host architecture is x86-64"
else
  warn "Host architecture is $(uname -m). x86-64 is recommended for QNX SDP host tools."
fi

echo ""
echo "[2/8] QNX SDP" | tee -a "${LOG_FILE}"
require_env QNX_HOST "Source the QNX SDP environment script (e.g. qnxsdp-env.sh)."
require_env QNX_TARGET "Source the QNX SDP environment script (e.g. qnxsdp-env.sh)."

if [[ -d "${QNX_HOST:-}" ]]; then
  ok "QNX_HOST directory exists"
else
  fail "QNX_HOST directory does not exist: ${QNX_HOST:-}"
fi

if [[ -d "${QNX_TARGET:-}" ]]; then
  ok "QNX_TARGET directory exists"
else
  fail "QNX_TARGET directory does not exist: ${QNX_TARGET:-}"
fi

if command -v qcc >/dev/null 2>&1; then
  ok "qcc found ($(qcc -V 2>&1 | head -n1 || true))"
else
  fail "qcc not found in PATH. Ensure QNX_HOST/usr/bin is on PATH."
fi

if command -v q++ >/dev/null 2>&1; then
  ok "q++ found"
else
  fail "q++ not found in PATH"
fi

# Verify sysroot contains expected Screen headers.
if [[ -f "${QNX_TARGET:-}/usr/include/screen/screen.h" ]]; then
  ok "QNX Screen headers present"
else
  fail "QNX Screen headers missing: \${QNX_TARGET}/usr/include/screen/screen.h"
fi

# Verify sysroot contains libscreen.
if [[ -f "${QNX_TARGET:-}/usr/lib/libscreen.so" || -f "${QNX_TARGET:-}/usr/lib/libscreen.a" ]]; then
  ok "QNX Screen library present"
else
  fail "QNX Screen library missing in \${QNX_TARGET}/usr/lib"
fi

# Verify EGL/GLES headers.
if [[ -f "${QNX_TARGET:-}/usr/include/EGL/egl.h" ]]; then
  ok "EGL headers present"
else
  warn "EGL headers not found; graphics backend will not build yet"
fi

echo ""
echo "[3/8] Build tools" | tee -a "${LOG_FILE}"
check_cmd cmake "3.28"
check_cmd ninja "1.11"

echo ""
echo "[4/8] Node.js tooling" | tee -a "${LOG_FILE}"
check_cmd node "24"
check_cmd pnpm "9"
if [[ -f "${PROJECT_ROOT}/package.json" ]]; then
  ok "package.json found"
else
  fail "package.json not found at project root"
fi

echo ""
echo "[5/8] Python" | tee -a "${LOG_FILE}"
if command -v python3 >/dev/null 2>&1; then
  ok "python3 found ($(python3 --version))"
else
  fail "python3 not found"
fi

echo ""
echo "[6/8] React Native / Expo / Nitro compatibility" | tee -a "${LOG_FILE}"
if [[ -f "${PROJECT_ROOT}/docs/getting-started/compatibility-matrix.md" ]]; then
  ok "compatibility-matrix.md found"
else
  fail "compatibility-matrix.md missing"
fi

# Future: verify package versions against the matrix.
ok "Package compatibility checks are documented in compatibility-matrix.md"

echo ""
echo "[7/8] Target device configuration" | tee -a "${LOG_FILE}"
if [[ -n "${QNX_TARGET_IP:-}" ]]; then
  ok "QNX_TARGET_IP=${QNX_TARGET_IP}"
else
  warn "QNX_TARGET_IP is not set. Deployment scripts require it."
fi

if [[ -n "${QNX_TARGET_USER:-}" ]]; then
  ok "QNX_TARGET_USER=${QNX_TARGET_USER}"
else
  warn "QNX_TARGET_USER is not set. Deployment scripts will default to root."
fi

echo ""
echo "[8/8] Project structure" | tee -a "${LOG_FILE}"
for dir in native packages apps scripts tests docs cmake; do
  if [[ -d "${PROJECT_ROOT}/${dir}" ]]; then
    ok "${dir}/ directory present"
  else
    fail "${dir}/ directory missing"
  fi
done

echo ""
echo "------------------------------" | tee -a "${LOG_FILE}"
if [[ ${ERRORS} -eq 0 ]]; then
  echo "Environment OK (${WARNINGS} warnings)." | tee -a "${LOG_FILE}"
  echo "Next step: ./scripts/configure-qnx.sh aarch64 Debug" | tee -a "${LOG_FILE}"
  exit 0
else
  echo "Environment has ${ERRORS} error(s) and ${WARNINGS} warning(s)." | tee -a "${LOG_FILE}"
  echo "See ${LOG_FILE} for details." | tee -a "${LOG_FILE}"
  exit 1
fi
