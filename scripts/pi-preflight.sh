#!/usr/bin/env bash
set -euo pipefail

# Preflight a current QNX Everywhere QSTI Raspberry Pi target before deploying.
# Defaults follow the stock image: qnxuser@qnxpi.local.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"

rnqnx_target_init

EVIDENCE_DIR="${RNQNX_PI_EVIDENCE_DIR:-${PROJECT_ROOT}/artifacts/pi-preflight/$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "${EVIDENCE_DIR}"

fail() {
  echo "ERROR: $1" >&2
  exit 1
}

echo "react-native-qnx Raspberry Pi preflight"
echo "Target: $(rnqnx_target_address)"
echo "Remote app dir: ${QNX_TARGET_DIR}"
echo "Evidence: ${EVIDENCE_DIR}"
echo

for tool in cmake ssh scp qcc q++; do
  command -v "${tool}" >/dev/null 2>&1 || fail "${tool} is not available on PATH."
done

[[ -n "${QNX_HOST:-}" ]] || fail "QNX_HOST is not set. Source qnxsdp-env.sh or set QNX_SDP_ENV before running the installer."
[[ -n "${QNX_TARGET:-}" ]] || fail "QNX_TARGET is not set. Source qnxsdp-env.sh or set QNX_SDP_ENV before running the installer."
[[ -d "${QNX_HOST}" ]] || fail "QNX_HOST does not exist: ${QNX_HOST}"
[[ -d "${QNX_TARGET}" ]] || fail "QNX_TARGET does not exist: ${QNX_TARGET}"

qcc -V >"${EVIDENCE_DIR}/qcc-V.txt" 2>&1
q++ -V >"${EVIDENCE_DIR}/qxx-V.txt" 2>&1
grep -Fq "gcc_ntoaarch64le" "${EVIDENCE_DIR}/qcc-V.txt" ||
  fail "The installed QNX compiler does not advertise gcc_ntoaarch64le."

echo "[host] QNX AArch64 compiler: ready"

REMOTE_REPORT="$(
  rnqnx_ssh "sh -s -- '${QNX_TARGET_DIR}'" <<'RNQNX_REMOTE'
set -e
target_dir="$1"
echo RNQNX_PI_PREFLIGHT=1
echo "uname=$(uname -a)"
echo "machine=$(uname -m)"
echo "node=$(uname -n)"
if [ "$(uname -m)" != "aarch64le" ]; then
  echo ERROR=expected-aarch64le
  exit 21
fi
if [ ! -d /dev/screen ]; then
  echo ERROR=screen-not-ready
  exit 22
fi
echo screen=ready
mkdir -p "$target_dir"
test -w "$target_dir"
echo target_dir=writable
for cmd in screenshot gles2-gears egl-configs slog2info pidin; do
  if command -v "$cmd" >/dev/null 2>&1; then
    echo "$cmd=present"
  else
    echo "$cmd=missing"
  fi
done
if command -v use >/dev/null 2>&1 && [ -f /system/bin/qconn ]; then
  echo qconn_version_begin
  use -i /system/bin/qconn 2>/dev/null || true
  echo qconn_version_end
fi
RNQNX_REMOTE
)"

printf '%s\n' "${REMOTE_REPORT}" | tee "${EVIDENCE_DIR}/target-preflight.txt"

grep -Fq "machine=aarch64le" "${EVIDENCE_DIR}/target-preflight.txt" ||
  fail "Target is not reporting QNX AArch64le."
grep -Fq "screen=ready" "${EVIDENCE_DIR}/target-preflight.txt" ||
  fail "QNX Screen is not ready on the target."
grep -Fq "target_dir=writable" "${EVIDENCE_DIR}/target-preflight.txt" ||
  fail "Target deployment directory is not writable."

if ! grep -Fq "screenshot=present" "${EVIDENCE_DIR}/target-preflight.txt"; then
  echo "WARNING: QNX screenshot utility is missing; the install can still run, but visual evidence will not be captured."
fi
if ! grep -Fq "gles2-gears=present" "${EVIDENCE_DIR}/target-preflight.txt"; then
  echo "WARNING: gles2-gears is missing; this does not block the Screen smoke app."
fi

echo
echo "Pi preflight passed."
