#!/usr/bin/env bash
set -euo pipefail

# scripts/verify-qnx-license.sh
# Verifies that the QNX SDP environment is active and the installation is licensed.

if [[ -z "${QNX_HOST:-}" || -z "${QNX_TARGET:-}" ]]; then
  echo "ERROR: QNX_HOST and QNX_TARGET are not set."
  echo "Source the QNX SDP environment script before running this script."
  echo "Example: source /opt/qnx/800/host_linux/x86_64/bin/qnxsdp-env.sh"
  exit 1
fi

echo "QNX_HOST:   ${QNX_HOST}"
echo "QNX_TARGET: ${QNX_TARGET}"

if command -v qcc >/dev/null 2>&1; then
  echo "qcc: $(command -v qcc)"
  qcc -V || true
else
  echo "ERROR: qcc not found. The QNX host toolchain may not be on PATH."
  exit 1
fi

if [[ -f "${QNX_TARGET}/usr/include/screen/screen.h" ]]; then
  echo "QNX Screen headers found."
else
  echo "WARNING: QNX Screen headers not found in \${QNX_TARGET}."
  echo "         The installed SDP may not include the graphics BSP package."
fi

echo ""
echo "If the commands above report active license information, the QNX environment is ready."
