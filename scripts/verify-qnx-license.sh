#!/usr/bin/env bash
set -euo pipefail

# scripts/verify-qnx-license.sh
# Verifies that the QNX SDP environment is active and the installation is licensed.

if [[ -z "${QNX_HOST:-}" || -z "${QNX_TARGET:-}" ]]; then
  echo "ERROR: QNX_HOST and QNX_TARGET are not set."
  echo "Source the QNX SDP environment script before running this script."
  echo "Source the installation-specific top-level qnxsdp-env.sh script."
  exit 1
fi

echo "QNX_HOST:   ${QNX_HOST}"
echo "QNX_TARGET: ${QNX_TARGET}"

if command -v qcc >/dev/null 2>&1; then
  echo "qcc: $(command -v qcc)"
  qcc -V
else
  echo "ERROR: qcc not found. The QNX host toolchain may not be on PATH."
  exit 1
fi

if command -v q++ >/dev/null 2>&1; then
  echo "q++: $(command -v q++)"
  q++ -V
else
  echo "ERROR: q++ not found. The QNX host toolchain may not be on PATH."
  exit 1
fi

if [[ -f "${QNX_TARGET}/usr/include/screen/screen.h" ]]; then
  echo "QNX Screen headers found."
else
  echo "WARNING: QNX Screen headers not found in \${QNX_TARGET}."
  echo "         The installed SDP may not include the graphics BSP package."
fi

echo ""
echo "Compiler target discovery passed. Run ./scripts/validate-g0-a.sh for an actual licensed compile/link and Pi execution."
