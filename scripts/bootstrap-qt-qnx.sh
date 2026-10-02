#!/usr/bin/env bash
set -euo pipefail

# Builds the minimum Qt 6 QNX runtime needed by react-native-qnx:
# qtbase + qtdeclarative -> Core, Gui, Qml, Quick.
#
# Required:
#   RNQNX_QT_SOURCE_DIR  Qt source checkout (for example Qt 6.11.3)
#   RNQNX_QT_HOST_PATH   matching host Qt installation used for moc/rcc/qml tools
#
# Optional:
#   RNQNX_QT_BUILD_DIR       local build directory
#   RNQNX_QT_QNX_PREFIX      local staging/install prefix
#   RNQNX_QT_TARGET_PREFIX   runtime prefix embedded in the QNX Qt build
#   RNQNX_QT_CONFIG          Release (default) or Debug
#
# The QNX Qt build is intentionally kept to qtbase + qtdeclarative. Those
# submodules contain every Qt module the current renderer links.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck source=scripts/lib/qnx-sdp.sh
source "${SCRIPT_DIR}/lib/qnx-sdp.sh"
rnqnx_source_sdp_environment

QT_SOURCE_DIR="${RNQNX_QT_SOURCE_DIR:-}"
QT_HOST_PATH="${RNQNX_QT_HOST_PATH:-}"
QT_BUILD_DIR="${RNQNX_QT_BUILD_DIR:-${PROJECT_ROOT}/build/qt-qnx-aarch64}"
QT_QNX_PREFIX="${RNQNX_QT_QNX_PREFIX:-${PROJECT_ROOT}/build/qt-qnx-aarch64-install}"
QT_TARGET_PREFIX="${RNQNX_QT_TARGET_PREFIX:-/opt/qt6}"
QT_CONFIG="${RNQNX_QT_CONFIG:-Release}"

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

[[ -n "${QT_SOURCE_DIR}" ]] ||
  fail "RNQNX_QT_SOURCE_DIR must point to a Qt source checkout."
[[ -x "${QT_SOURCE_DIR}/configure" ]] ||
  fail "Qt configure script not found: ${QT_SOURCE_DIR}/configure"
[[ -n "${QT_HOST_PATH}" ]] ||
  fail "RNQNX_QT_HOST_PATH must point to the matching host Qt installation."
[[ -x "${QT_HOST_PATH}/bin/moc" ]] ||
  fail "Host Qt moc not found: ${QT_HOST_PATH}/bin/moc"
[[ "${QT_CONFIG}" == "Release" || "${QT_CONFIG}" == "Debug" ]] ||
  fail "RNQNX_QT_CONFIG must be Release or Debug."
command -v cmake >/dev/null 2>&1 || fail "cmake is required."
command -v ninja >/dev/null 2>&1 || fail "ninja is required."
command -v qcc >/dev/null 2>&1 || fail "qcc is required after activating QNX SDP."
command -v q++ >/dev/null 2>&1 || fail "q++ is required after activating QNX SDP."

mkdir -p "${QT_BUILD_DIR}" "${QT_QNX_PREFIX}"

case "${QT_CONFIG}" in
  Release) qt_build_flag="-release" ;;
  Debug) qt_build_flag="-debug" ;;
esac

echo "=================================================="
echo " react-native-qnx · Qt for QNX bootstrap"
echo "=================================================="
echo "Qt source:       ${QT_SOURCE_DIR}"
echo "Host Qt:         ${QT_HOST_PATH}"
echo "Build directory: ${QT_BUILD_DIR}"
echo "Staging prefix:  ${QT_QNX_PREFIX}"
echo "Target prefix:   ${QT_TARGET_PREFIX}"
echo "Configuration:   ${QT_CONFIG}"
echo

echo "[1/3] Configuring Qt (qtbase + qtdeclarative)"
cd "${QT_BUILD_DIR}"
"${QT_SOURCE_DIR}/configure"   "${qt_build_flag}"   -shared   -nomake examples   -nomake tests   -submodules qtbase,qtdeclarative   -qt-host-path "${QT_HOST_PATH}"   -extprefix "${QT_QNX_PREFIX}"   -prefix "${QT_TARGET_PREFIX}"   --   -DCMAKE_TOOLCHAIN_FILE="${PROJECT_ROOT}/vnext/cmake/qnx-aarch64le.toolchain.cmake"   -DCMAKE_BUILD_TYPE="${QT_CONFIG}"

echo
echo "[2/3] Building Qt for QNX AArch64"
cmake --build "${QT_BUILD_DIR}" --parallel

echo
echo "[3/3] Installing Qt into the local staging prefix"
cmake --install "${QT_BUILD_DIR}"

QT_CONFIG_FILE="${QT_QNX_PREFIX}/lib/cmake/Qt6/Qt6Config.cmake"
QT_TOOLCHAIN_FILE="${QT_QNX_PREFIX}/lib/cmake/Qt6/qt.toolchain.cmake"
[[ -f "${QT_CONFIG_FILE}" ]] ||
  fail "Qt install did not produce ${QT_CONFIG_FILE}"
[[ -f "${QT_TOOLCHAIN_FILE}" ]] ||
  fail "Qt install did not produce ${QT_TOOLCHAIN_FILE}"

for module in Core Gui Qml Quick; do
  if ! find "${QT_QNX_PREFIX}/lib" -maxdepth 1     \( -name "libQt6${module}.so" -o -name "libQt6${module}.so.*" \)     -print -quit | grep -q .; then
    fail "Qt ${module} runtime library was not installed."
  fi
done

ENV_FILE="${QT_QNX_PREFIX}/rnqnx-qt-qnx.env"
cat >"${ENV_FILE}" <<ENV
export RNQNX_QT_QNX_PREFIX="${QT_QNX_PREFIX}"
export RNQNX_QT_TARGET_PREFIX="${QT_TARGET_PREFIX}"
export RNQNX_QT_TOOLCHAIN_FILE="${QT_TOOLCHAIN_FILE}"
export RNQNX_QT_HOST_PATH="${QT_HOST_PATH}"
ENV

echo
echo "Qt for QNX bootstrap complete."
echo "Environment file: ${ENV_FILE}"
echo "Use: source ${ENV_FILE}"
