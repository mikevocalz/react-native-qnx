#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ARCH="${1:-aarch64}"
CONFIG="${2:-MinSizeRel}"

case "${ARCH}" in
  aarch64|x86_64) ;;
  *) echo "Usage: $0 <aarch64|x86_64> [Debug|Release|MinSizeRel]" >&2; exit 1 ;;
esac
case "${CONFIG}" in
  Debug|Release|MinSizeRel|RelWithDebInfo) ;;
  *) echo "Usage: $0 <aarch64|x86_64> [Debug|Release|MinSizeRel]" >&2; exit 1 ;;
esac

[[ -n "${QNX_HOST:-}" ]] || { echo "ERROR: QNX_HOST is not set" >&2; exit 1; }
[[ -n "${QNX_TARGET:-}" ]] || { echo "ERROR: QNX_TARGET is not set" >&2; exit 1; }

SOURCE_DIR="${RNQNX_HERMES_SOURCE_DIR:-${PROJECT_ROOT}/third-party/hermes-260318099.0.5}"
BUILD_DIR="${RNQNX_HERMES_BUILD_DIR:-${PROJECT_ROOT}/build/hermes-qnx-${ARCH}}"
IMPORT_FILE="${PROJECT_ROOT}/cmake/ImportHermesHostCompilers.cmake"

if [[ ! -f "${SOURCE_DIR}/.rnqnx-source-tag" ]]; then
  bash "${SCRIPT_DIR}/prepare-hermes-qnx.sh"
fi

resolve_hermesc() {
  if [[ -n "${RNQNX_HOST_HERMESC:-}" ]]; then
    printf '%s\n' "${RNQNX_HOST_HERMESC}"
    return
  fi

  local root
  root="$(
    node -e "const p=require('path'); console.log(p.dirname(require.resolve('hermes-compiler/package.json')))"
  )"

  local candidates=()
  case "$(uname -s)" in
    Darwin)
      candidates+=("${root}/hermesc/osx-bin/hermesc")
      ;;
    Linux)
      candidates+=("${root}/hermesc/linux64-bin/hermesc")
      ;;
    MINGW*|MSYS*|CYGWIN*)
      candidates+=("${root}/hermesc/win64-bin/hermesc.exe")
      candidates+=("${root}/hermesc/linux64-bin/hermesc")
      ;;
  esac

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -f "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return
    fi
  done

  return 1
}

HOST_HERMESC="$(resolve_hermesc)" || {
  echo "ERROR: Could not locate host hermesc from hermes-compiler." >&2
  echo "Set RNQNX_HOST_HERMESC to a runnable compiler matching the installed React Native line." >&2
  exit 1
}

[[ -f "${HOST_HERMESC}" ]] || { echo "ERROR: hermesc not found: ${HOST_HERMESC}" >&2; exit 1; }

TOOLCHAIN="${PROJECT_ROOT}/cmake/toolchains/qnx-8.0.cmake"

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -G Ninja   -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}"   -DQNX_TARGET_ARCH="${ARCH}"   -DCMAKE_BUILD_TYPE="${CONFIG}"   -DIMPORT_HOST_COMPILERS="${IMPORT_FILE}"   -DRNQNX_HOST_HERMESC="${HOST_HERMESC}"   -DJSI_DIR="${PROJECT_ROOT}/node_modules/react-native/ReactCommon/jsi"   -DHERMES_ENABLE_INTL=OFF   -DHERMES_UNICODE_LITE=ON   -DHERMES_ENABLE_TEST_SUITE=OFF   -DHERMES_ENABLE_DEBUGGER=OFF   -DHERMES_ENABLE_TOOLS=OFF   -DHERMES_BUILD_LEAN_LIBHERMES=ON   -DHERMES_BUILD_SHARED_JSI=OFF   -DHERMESVM_INTERNAL_JAVASCRIPT_NATIVE=OFF   -DHERMESVM_ALLOW_JIT=0   -DHERMES_ALLOW_BOOST_CONTEXT=0   -DCMAKE_SIZEOF_VOID_P=8

echo "Configured Hermes for QNX:"
echo "  source:  ${SOURCE_DIR}"
echo "  build:   ${BUILD_DIR}"
echo "  arch:    ${ARCH}"
echo "  config:  ${CONFIG}"
echo "  hermesc: ${HOST_HERMESC}"
echo
echo "Build with: cmake --build ${BUILD_DIR} --parallel --target hermesvmlean jsi"
