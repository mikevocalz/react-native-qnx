#!/bin/sh
# Builds rnqnx-winmgr for the Raspberry Pi 400 (aarch64le) with the QNX SDP.
#   tools/qnx-winmgr/build.sh [output]   (default: build/qnx-winmgr/rnqnx-winmgr)
# Needs QNX_TARGET, i.e. `source <sdp>/qnxsdp-env.sh` first.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=${1:-build/qnx-winmgr/rnqnx-winmgr}
: "${QNX_TARGET:?source qnxsdp-env.sh first}"
mkdir -p "$(dirname "$out")"
qcc -Vgcc_ntoaarch64le -O2 -Wall -Wextra -Werror -Wno-format-truncation \
  -I"$QNX_TARGET/usr/include/freetype2" -I"$QNX_TARGET/usr/include/libpng16" \
  -o "$out" "$here/rnqnx-winmgr.c" -lscreen -lfreetype -lpng16
echo "$out"
