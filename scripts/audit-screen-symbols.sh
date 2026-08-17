#!/usr/bin/env bash
set -euo pipefail

# Audit the Screen symbols used by QnxScreenSurface against the installed QNX
# Screen header. This script is a developer convenience, not part of CI.

# Save the host grep path before sourcing qnxsdp-env.sh, because that script
# prepends the QNX host bin directory to PATH and the QNX grep has different
# path semantics on Windows/Git Bash.
HOST_GREP="$(command -v grep)"

if [[ -z "${QNX_TARGET:-}" ]]; then
  if [[ -f /c/Users/Owner/qnx800/qnxsdp-env.sh ]]; then
    source /c/Users/Owner/qnx800/qnxsdp-env.sh
  elif [[ -f /cygdrive/c/Users/Owner/qnx800/qnxsdp-env.sh ]]; then
    source /cygdrive/c/Users/Owner/qnx800/qnxsdp-env.sh
  fi
fi

HDR="${QNX_TARGET}/usr/include/screen/screen.h"
if [[ ! -f "$HDR" ]]; then
  echo "ERROR: screen.h not found: $HDR" >&2
  exit 1
fi

SYMS=(
  screen_create_context
  screen_create_window_type
  screen_set_window_property_iv
  screen_get_window_property_iv
  screen_get_window_property_pv
  screen_create_window_buffers
  screen_post_window
  screen_fill
  screen_destroy_window
  screen_destroy_context
  SCREEN_APPLICATION_CONTEXT
  SCREEN_APPLICATION_WINDOW
  SCREEN_PROPERTY_USAGE
  SCREEN_PROPERTY_FORMAT
  SCREEN_PROPERTY_BUFFER_SIZE
  SCREEN_PROPERTY_SIZE
  SCREEN_PROPERTY_RENDER_BUFFER_COUNT
  SCREEN_PROPERTY_RENDER_BUFFERS
  SCREEN_USAGE_NATIVE
  SCREEN_FORMAT_RGBA8888
  SCREEN_BLIT_COLOR
  SCREEN_BLIT_DESTINATION_X
  SCREEN_BLIT_DESTINATION_Y
  SCREEN_BLIT_DESTINATION_WIDTH
  SCREEN_BLIT_DESTINATION_HEIGHT
  SCREEN_BLIT_END
)

MISSING=0
for s in "${SYMS[@]}"; do
  if "${HOST_GREP}" -q "\\b${s}\\b" "$HDR"; then
    echo "OK: $s"
  else
    echo "MISSING: $s"
    MISSING=$((MISSING + 1))
  fi
done

if ((MISSING > 0)); then
  echo "ERROR: $MISSING Screen symbol(s) missing from $HDR" >&2
  exit 1
fi

echo "All used Screen symbols are present in $HDR."
