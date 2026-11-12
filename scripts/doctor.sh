#!/usr/bin/env bash
set -euo pipefail

# scripts/doctor.sh
# Validates the development environment for react-native-qnx.
# Exit code 0 means the environment is ready to configure/build for QNX.
# Every ERROR and WARNING line is followed by a "Fix:" line naming the next
# command or action. Checks that depend on a failed prerequisite are reported
# as SKIPPED instead of repeating the same root cause.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# RNQNX_DOCTOR_PROJECT_ROOT lets tests point the doctor at a fixture checkout.
PROJECT_ROOT="${RNQNX_DOCTOR_PROJECT_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
LOG_DIR="${RNQNX_DOCTOR_LOG_DIR:-${PROJECT_ROOT}/artifacts/logs}"
LOG_FILE="${LOG_DIR}/$(date +%Y-%m-%d)-qnx-doctor.txt"
GUIDE="docs/getting-started/build-and-deploy.md"

mkdir -p "${LOG_DIR}"

echo "react-native-qnx doctor" | tee "${LOG_FILE}"
echo "Started at $(date -u +%Y-%m-%dT%H:%M:%SZ)" | tee -a "${LOG_FILE}"
echo "Log: ${LOG_FILE}"
echo ""

ERRORS=0
WARNINGS=0
FIRST_FIX=""

function remember_fix() {
  if [[ -z "${FIRST_FIX}" && -n "${1:-}" ]]; then
    FIRST_FIX="$1"
  fi
}

# fail <what happened> [fix]
function fail() {
  echo "  ERROR: $1" | tee -a "${LOG_FILE}"
  if [[ -n "${2:-}" ]]; then
    echo "    Fix: $2" | tee -a "${LOG_FILE}"
  fi
  remember_fix "${2:-}"
  ERRORS=$((ERRORS + 1))
}

# warn <what happened> [fix]
function warn() {
  echo "  WARNING: $1" | tee -a "${LOG_FILE}"
  if [[ -n "${2:-}" ]]; then
    echo "    Fix: $2" | tee -a "${LOG_FILE}"
  fi
  WARNINGS=$((WARNINGS + 1))
}

function ok() {
  echo "  OK: $1" | tee -a "${LOG_FILE}"
}

# skipped <check> <reason>
function skipped() {
  echo "  SKIPPED: $1 ($2)" | tee -a "${LOG_FILE}"
}

function version_at_least() {
  local current="$1"
  local minimum="$2"
  local current_parts
  local minimum_parts
  local index
  IFS='.' read -r -a current_parts <<< "${current}"
  IFS='.' read -r -a minimum_parts <<< "${minimum}"
  for index in 0 1 2 3; do
    local current_part=$((10#${current_parts[${index}]:-0}))
    local minimum_part=$((10#${minimum_parts[${index}]:-0}))
    if (( current_part > minimum_part )); then
      return 0
    fi
    if (( current_part < minimum_part )); then
      return 1
    fi
  done
  return 0
}

# check_cmd <name> <minimum version or empty> <fix>
function check_cmd() {
  local name="$1"
  local min_version="${2:-}"
  local fix="${3:-}"
  if ! command -v "${name}" >/dev/null 2>&1; then
    fail "${name} is not installed or not on PATH." "${fix}"
    return
  fi

  local version_output
  local exit_code
  set +e
  version_output="$("${name}" --version 2>&1)"
  exit_code=$?
  set -e

  if [[ ${exit_code} -ne 0 ]]; then
    fail "${name} --version failed: $(printf '%s\n' "${version_output}" | head -n1)" "${fix}"
    return
  fi

  local version_line
  version_line="$(printf '%s\n' "${version_output}" | head -n1)"
  if [[ -n "${min_version}" ]]; then
    local version
    version="$(printf '%s\n' "${version_line}" | grep -Eo '[0-9]+(\.[0-9]+)+' | head -n1 || true)"
    if [[ -z "${version}" ]]; then
      fail "Could not read the ${name} version from: ${version_line}" "${fix}"
    elif version_at_least "${version}" "${min_version}"; then
      ok "${name} ${version} satisfies >=${min_version}"
    else
      fail "${name} ${version} is too old; this repository needs >=${min_version}." "${fix}"
    fi
  else
    ok "${name} found (${version_line})"
  fi
}

case "$(uname -s)" in
  Linux) BOOTSTRAP="./scripts/bootstrap-host.sh" ;;
  Darwin) BOOTSTRAP="./scripts/bootstrap-macos.sh" ;;
  *) BOOTSTRAP="the packages listed in docs/toolchain.md" ;;
esac
SDP_FIX="source <qnx-installation>/qnxsdp-env.sh in this shell, then rerun pnpm qnx doctor (see ${GUIDE})"

echo "[1/8] Host environment" | tee -a "${LOG_FILE}"
HOST_OS="$(uname -s)"
if [[ "${HOST_OS}" == "Linux" ]]; then
  ok "Host OS is Linux ($(uname -srm))"
else
  warn "Host OS is ${HOST_OS}. QNX SDP 8.0 ships Linux and Windows host tools only, so qcc cannot run here." \
    "use this host for JavaScript work and host validation; run configure/build on x86-64 Ubuntu 22.04 (${GUIDE}, \"macOS remote-development option\")"
fi

if [[ "$(uname -m)" == "x86_64" ]]; then
  ok "Host architecture is x86-64"
else
  warn "Host architecture is $(uname -m). The QNX SDP host tools are x86-64 binaries." \
    "build QNX targets on an x86-64 Ubuntu 22.04 host"
fi

echo ""
echo "[2/8] QNX SDP" | tee -a "${LOG_FILE}"
SDP_READY=1
if [[ -z "${QNX_HOST:-}" || -z "${QNX_TARGET:-}" ]]; then
  SDP_READY=0
  fail "The QNX SDP environment is not loaded (QNX_HOST and QNX_TARGET are unset)." "${SDP_FIX}"
else
  ok "QNX_HOST=${QNX_HOST}"
  ok "QNX_TARGET=${QNX_TARGET}"
  if [[ ! -d "${QNX_HOST}" ]]; then
    SDP_READY=0
    fail "QNX_HOST points to a missing directory: ${QNX_HOST}" "${SDP_FIX}"
  fi
  if [[ ! -d "${QNX_TARGET}" ]]; then
    SDP_READY=0
    fail "QNX_TARGET points to a missing directory: ${QNX_TARGET}" "${SDP_FIX}"
  fi
fi

if [[ ${SDP_READY} -eq 1 ]]; then
  for tool in qcc q++; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
      if [[ -x "${QNX_HOST}/usr/bin/${tool}" ]]; then
        fail "${tool} exists in \${QNX_HOST}/usr/bin but that directory is not on PATH." \
          "export PATH=\"\${QNX_HOST}/usr/bin:\${PATH}\", or source qnxsdp-env.sh again"
      elif [[ "${HOST_OS}" != "Linux" ]]; then
        fail "${tool} is not available for ${HOST_OS}: QNX_HOST selects ${QNX_HOST}, which has no runnable ${tool}." \
          "run QNX configure/build on x86-64 Ubuntu 22.04 with the SDP installed there (${GUIDE})"
      else
        fail "${tool} is missing from \${QNX_HOST}/usr/bin." \
          "repair or reinstall QNX SDP 8.0.5 with QNX Software Center, then ${SDP_FIX}"
      fi
      continue
    fi
    if TOOL_OUTPUT="$("${tool}" -V 2>&1)"; then
      ok "${tool} target list is readable ($(printf '%s\n' "${TOOL_OUTPUT}" | head -n1))"
    else
      fail "${tool} -V failed: $(printf '%s\n' "${TOOL_OUTPUT}" | head -n1)" \
        "check the QNX license with ./scripts/verify-qnx-license.sh"
    fi
  done

  # Verify sysroot contains expected Screen headers.
  if [[ -f "${QNX_TARGET}/usr/include/screen/screen.h" ]]; then
    ok "QNX Screen headers present"
  else
    fail "QNX Screen headers missing: \${QNX_TARGET}/usr/include/screen/screen.h" \
      "install the Screen/graphics development package for your board in QNX Software Center"
  fi

  # Verify the sysroot contains libscreen. Its architecture directory is BSP-specific.
  SCREEN_LIBRARY="$(find "${QNX_TARGET}" \( -type f -o -type l \) \( -name 'libscreen.so' -o -name 'libscreen.a' \) -print -quit 2>/dev/null || true)"
  if [[ -n "${SCREEN_LIBRARY}" ]]; then
    ok "QNX Screen library present (${SCREEN_LIBRARY})"
  else
    fail "QNX Screen library missing below \${QNX_TARGET}" \
      "install the Screen/graphics development package for your board in QNX Software Center"
  fi

  # Verify EGL/GLES headers.
  if [[ -f "${QNX_TARGET}/usr/include/EGL/egl.h" ]]; then
    ok "EGL headers present"
  else
    warn "EGL headers not found; the graphics backend will not build yet." \
      "install the EGL/GLES development package for your board in QNX Software Center"
  fi
else
  for check in "qcc/q++" "Screen headers and library" "EGL headers"; do
    skipped "${check}" "QNX SDP environment is not loaded"
  done
fi

echo ""
echo "[3/8] Build tools" | tee -a "${LOG_FILE}"
check_cmd cmake "3.31.9" "install CMake 3.31.9 or newer with ${BOOTSTRAP}"
check_cmd ninja "1.11" "install Ninja 1.11 or newer with ${BOOTSTRAP}"

# Qt host tools: needed by the host renderer and as QT_HOST_PATH for the QNX
# React Native runtime. The rnqnx_playground Screen probe does not use Qt.
QT_MINIMUM="6.11.1"
QT_TOOL=""
for candidate in qtpaths6 qtpaths qmake6 qmake; do
  if command -v "${candidate}" >/dev/null 2>&1; then
    QT_TOOL="${candidate}"
    break
  fi
done
QT_FIX="install Qt ${QT_MINIMUM} or newer host tools (brew install qt on macOS, or the Qt online installer) and put its bin directory on PATH; see docs/toolchain.md"
if [[ -z "${QT_TOOL}" ]]; then
  warn "Qt host tools (qtpaths/qmake) not found. The host renderer and rnqnx_playground_qnx need them; the Screen probe does not." \
    "${QT_FIX}"
else
  set +e
  if [[ "${QT_TOOL}" == qtpaths* ]]; then
    QT_VERSION="$("${QT_TOOL}" --qt-version 2>/dev/null | head -n1)"
  else
    QT_VERSION="$("${QT_TOOL}" -query QT_VERSION 2>/dev/null | head -n1)"
  fi
  set -e
  if [[ -z "${QT_VERSION}" ]]; then
    warn "Could not read the Qt version from ${QT_TOOL}." "${QT_FIX}"
  elif version_at_least "${QT_VERSION}" "${QT_MINIMUM}"; then
    ok "Qt ${QT_VERSION} host tools found (${QT_TOOL})"
  else
    warn "Qt ${QT_VERSION} host tools are older than ${QT_MINIMUM}." "${QT_FIX}"
  fi
fi

echo ""
echo "[4/8] Node.js tooling" | tee -a "${LOG_FILE}"
check_cmd node "26.0.0" "install Node.js 26 or newer from https://nodejs.org (or your version manager), then open a new shell"
# The pin comes from package.json "packageManager", so a pnpm bump needs no edit here.
# A missing package.json must not abort here: the check below reports it.
PNPM_PIN="$(sed -n 's/.*"packageManager": *"pnpm@\([^"]*\)".*/\1/p' "${PROJECT_ROOT}/package.json" 2>/dev/null || true)"
# packageManager may carry an integrity suffix such as 12.18.0+sha512.<hash>.
PNPM_PIN="${PNPM_PIN%%+*}"
PNPM_PIN="${PNPM_PIN:-12.18.0}"
PNPM_FIX="install pnpm ${PNPM_PIN}: npm install -g pnpm@${PNPM_PIN} (Node 26 does not bundle Corepack)"
if ! command -v pnpm >/dev/null 2>&1; then
  fail "pnpm is not installed or not on PATH." "${PNPM_FIX}"
else
  set +e
  PNPM_ACTUAL="$(pnpm --version 2>&1 | head -n1 | sed 's/^v//')"
  set -e
  if [[ "${PNPM_ACTUAL}" == "${PNPM_PIN}" ]]; then
    ok "pnpm ${PNPM_ACTUAL} matches the repository pin"
  else
    fail "pnpm reported '${PNPM_ACTUAL}', but this repository pins ${PNPM_PIN}." "${PNPM_FIX}"
  fi
fi
if [[ -f "${PROJECT_ROOT}/package.json" ]]; then
  ok "package.json found"
else
  fail "package.json not found at project root" "run pnpm qnx doctor from a react-native-qnx checkout"
fi

echo ""
echo "[5/8] Python" | tee -a "${LOG_FILE}"
check_cmd python3 "3.11" "install Python 3.11 or newer with ${BOOTSTRAP}"

echo ""
echo "[6/8] React Native / Expo / Nitro compatibility" | tee -a "${LOG_FILE}"
if [[ -f "${PROJECT_ROOT}/docs/getting-started/compatibility-matrix.md" ]]; then
  ok "compatibility-matrix.md found"
else
  fail "compatibility-matrix.md missing" "restore the file with git checkout -- docs/getting-started/compatibility-matrix.md"
fi

if [[ -d "${PROJECT_ROOT}/node_modules" ]]; then
  if node "${PROJECT_ROOT}/scripts/validate-dependencies.mjs" >>"${LOG_FILE}" 2>&1; then
    ok "Installed dependency pins and source contracts validated"
  else
    fail "Installed dependencies do not match the repository pins." \
      "run pnpm install --frozen-lockfile, then pnpm validate:dependencies for details"
  fi
else
  fail "node_modules is missing." "run pnpm install --frozen-lockfile from the repository root"
fi

echo ""
echo "[7/8] Target device configuration" | tee -a "${LOG_FILE}"
if [[ -z "${QNX_TARGET_IP:-}" && -z "${QNX_TARGET_USER:-}" ]]; then
  skipped "target device" "no target configured; configure and build do not need one. Before deploy-qnx, export QNX_TARGET_IP and QNX_TARGET_USER"
else
  if [[ -n "${QNX_TARGET_IP:-}" ]]; then
    ok "QNX_TARGET_IP=${QNX_TARGET_IP}"
  else
    warn "QNX_TARGET_USER is set but QNX_TARGET_IP is not; deploy-qnx needs both." \
      "export QNX_TARGET_IP=<board address>, for example qnxpi.local"
  fi
  if [[ -n "${QNX_TARGET_USER:-}" ]]; then
    ok "QNX_TARGET_USER=${QNX_TARGET_USER}"
  else
    warn "QNX_TARGET_IP is set but QNX_TARGET_USER is not; deploy-qnx needs an explicit, verified SSH user." \
      "export QNX_TARGET_USER=<user>, for example qnxuser on the stock QSTI image"
  fi
fi

echo ""
echo "[8/8] Project structure" | tee -a "${LOG_FILE}"
for dir in native packages apps scripts tests docs cmake; do
  if [[ -d "${PROJECT_ROOT}/${dir}" ]]; then
    ok "${dir}/ directory present"
  else
    fail "${dir}/ directory missing" "use a complete clone of https://github.com/mikevocalz/react-native-qnx"
  fi
done

echo ""
echo "------------------------------" | tee -a "${LOG_FILE}"
if [[ ${ERRORS} -eq 0 ]]; then
  echo "Environment OK (${WARNINGS} warnings)." | tee -a "${LOG_FILE}"
  echo "Next step: pnpm qnx configure-qnx --arch aarch64 --config Debug" | tee -a "${LOG_FILE}"
  exit 0
else
  echo "Environment has ${ERRORS} error(s) and ${WARNINGS} warning(s)." | tee -a "${LOG_FILE}"
  echo "Start with: ${FIRST_FIX}" | tee -a "${LOG_FILE}"
  echo "Full log: ${LOG_FILE}" | tee -a "${LOG_FILE}"
  exit 1
fi
