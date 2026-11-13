#!/bin/sh
# Builds rnqnx-inject for the Raspberry Pi 400 (aarch64le) with the QNX SDP.
#   tools/qnx-input/build.sh [output]   (default: build/qnx-input/rnqnx-inject)
# Needs QNX_TARGET, i.e. `source <sdp>/qnxsdp-env.sh` first.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=${1:-build/qnx-input/rnqnx-inject}
: "${QNX_TARGET:?source qnxsdp-env.sh first}"
mkdir -p "$(dirname "$out")"
qcc -Vgcc_ntoaarch64le -O2 -Wall -Wextra -Werror -o "$out" "$here/rnqnx-inject.c" -lscreen
echo "$out"
