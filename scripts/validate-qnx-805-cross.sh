#!/usr/bin/env bash
set -euo pipefail

# Validate the current QNX AArch64 cross-build lane and capture reproducible
# evidence. This proves host/toolchain compilation only; it does not substitute
# for running the binaries on a physical QSTI target.
#
# Current project target baseline: QNX SDP 8.0.5.
# _NTO_VERSION identifies the OS major/minor ABI line, not the installed QNX
# Software Center patch level, so this script records (rather than fabricates)
# patch provenance separately.
#
# Optional:
#   RNQNX_QNX_SDP_LABEL=8.0.5
#   RNQNX_QNX_EVIDENCE_DIR=/path/to/evidence
#   RNQNX_QNX_CONFIGS="Debug Release"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

: "${RNQNX_QNX_SDP_LABEL:=8.0.5}"
: "${RNQNX_QNX_CONFIGS:=Debug Release}"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
EVIDENCE_DIR="${RNQNX_QNX_EVIDENCE_DIR:-${PROJECT_ROOT}/artifacts/qnx-cross/${STAMP}}"

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

mkdir -p "${EVIDENCE_DIR}"

[[ -n "${QNX_HOST:-}" ]] || fail "QNX_HOST is not set. Source qnxsdp-env.sh first."
[[ -n "${QNX_TARGET:-}" ]] || fail "QNX_TARGET is not set. Source qnxsdp-env.sh first."
[[ -d "${QNX_HOST}" ]] || fail "QNX_HOST does not exist: ${QNX_HOST}"
[[ -d "${QNX_TARGET}" ]] || fail "QNX_TARGET does not exist: ${QNX_TARGET}"

for cmd in cmake ninja qcc q++; do
  command -v "${cmd}" >/dev/null 2>&1 || fail "${cmd} is not available on PATH."
done

NTO_VERSION_HEADER="${QNX_TARGET}/usr/include/sys/nto_version.h"
[[ -f "${NTO_VERSION_HEADER}" ]] || fail "Missing QNX version header: ${NTO_VERSION_HEADER}"

NTO_VERSION="$(
  awk '$1 == "#define" && $2 == "_NTO_VERSION" { print $3; exit }' "${NTO_VERSION_HEADER}"
)"
[[ "${NTO_VERSION}" =~ ^[0-9]+$ ]] || fail "Could not parse _NTO_VERSION from ${NTO_VERSION_HEADER}"
(( NTO_VERSION >= 800 )) || fail "QNX target ABI is older than QNX 8: _NTO_VERSION=${NTO_VERSION}"

printf '%s\n' "${RNQNX_QNX_SDP_LABEL}" >"${EVIDENCE_DIR}/expected-sdp-label.txt"
printf '%s\n' "${NTO_VERSION}" >"${EVIDENCE_DIR}/nto-version.txt"
printf '%s\n' "${QNX_HOST}" >"${EVIDENCE_DIR}/qnx-host.txt"
printf '%s\n' "${QNX_TARGET}" >"${EVIDENCE_DIR}/qnx-target.txt"
qcc -V >"${EVIDENCE_DIR}/qcc-V.txt" 2>&1
q++ -V >"${EVIDENCE_DIR}/qxx-V.txt" 2>&1
cmake --version >"${EVIDENCE_DIR}/cmake-version.txt"
ninja --version >"${EVIDENCE_DIR}/ninja-version.txt"

if command -v qconfig >/dev/null 2>&1; then
  qconfig -a >"${EVIDENCE_DIR}/qconfig-a.txt" 2>&1 || true
fi

grep -Fq "gcc_ntoaarch64le" "${EVIDENCE_DIR}/qcc-V.txt" ||
  fail "qcc does not advertise gcc_ntoaarch64le."
grep -Fq "gcc_ntoaarch64le" "${EVIDENCE_DIR}/qxx-V.txt" ||
  fail "q++ does not advertise gcc_ntoaarch64le."

echo "QNX AArch64 cross-build validation"
echo "Expected SDP label: ${RNQNX_QNX_SDP_LABEL}"
echo "Target ABI manifest: _NTO_VERSION=${NTO_VERSION}"
echo "Evidence: ${EVIDENCE_DIR}"
echo

bash "${SCRIPT_DIR}/audit-screen-symbols.sh" | tee "${EVIDENCE_DIR}/screen-symbol-audit.txt"

read -r -a CONFIGS <<< "${RNQNX_QNX_CONFIGS}"
for config in "${CONFIGS[@]}"; do
  case "${config}" in
    Debug|Release) ;;
    *) fail "Unsupported configuration in RNQNX_QNX_CONFIGS: ${config}" ;;
  esac

  preset="qnx-aarch64-$(printf '%s' "${config}" | tr '[:upper:]' '[:lower:]')"
  build_dir="${PROJECT_ROOT}/build/${preset}"

  echo
  echo "==> ${preset}"
  bash "${SCRIPT_DIR}/configure-qnx.sh" aarch64 "${config}"
  cmake --build --preset "${preset}" --parallel --target     rnqnx_g0_cxx20_probe qnx_platform rnqnx_playground

  for binary in rnqnx_g0_cxx20_probe rnqnx_playground; do
    path="${build_dir}/bin/${binary}"
    [[ -f "${path}" ]] || fail "Missing expected binary: ${path}"

    readelf_cmd=""
    if command -v ntoaarch64-readelf >/dev/null 2>&1; then
      readelf_cmd="$(command -v ntoaarch64-readelf)"
    elif command -v aarch64-unknown-nto-qnx8.0.0-readelf >/dev/null 2>&1; then
      readelf_cmd="$(command -v aarch64-unknown-nto-qnx8.0.0-readelf)"
    else
      fail "No QNX AArch64 readelf tool is available on PATH."
    fi

    "${readelf_cmd}" -h "${path}" >"${EVIDENCE_DIR}/${preset}-${binary}-elf.txt"
    grep -Eq 'Machine:[[:space:]]+AArch64' "${EVIDENCE_DIR}/${preset}-${binary}-elf.txt" ||
      fail "${binary} is not an AArch64 ELF."

    if command -v ntoaarch64-objdump >/dev/null 2>&1; then
      ntoaarch64-objdump -p "${path}" >"${EVIDENCE_DIR}/${preset}-${binary}-dynamic.txt" || true
    fi
  done
done

cat >"${EVIDENCE_DIR}/README.txt" <<EOF
react-native-qnx current QNX cross-build evidence
completed_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)
expected_sdp_label=${RNQNX_QNX_SDP_LABEL}
nto_version=${NTO_VERSION}
configs=${RNQNX_QNX_CONFIGS}

Important:
_NTO_VERSION proves the QNX 8 target ABI line but does not independently prove
which QNX Software Center patch level is installed. The expected SDP label is
recorded as operator provenance. Attach QNX Software Center/package evidence
when promoting this run to an SDP 8.0.5 validation record.

This directory proves cross-compilation only. It does not prove physical QSTI
runtime behavior.
EOF

echo
echo "QNX AArch64 cross-build validation passed."
echo "Evidence: ${EVIDENCE_DIR}"
