# shellcheck shell=bash
# Shared selection for the QNX plugin recipe and corresponding-source manifest.
# Sourced by callers running with set -euo pipefail.
QNX_SOFTWARE_GRAB=${QNX_SOFTWARE_GRAB-0}
case "$QNX_SOFTWARE_GRAB" in
  0|1) ;;
  *) echo 'QNX_SOFTWARE_GRAB must be 0 or 1' >&2; exit 1 ;;
esac
patches=(qtbase-qnx-offscreen-surface.patch qtbase-qnx-resize-swap.patch)
if [ "$QNX_SOFTWARE_GRAB" = 1 ]; then
  patches+=(qtbase-qnx-software-grab.patch)
fi
export QNX_SOFTWARE_GRAB
