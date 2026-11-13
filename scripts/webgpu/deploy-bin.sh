#!/usr/bin/env bash
# Copies files to a NEW directory on the Pi and prints nothing else.
#   scripts/webgpu/deploy-bin.sh <remote-dir> <file>...
# Connection details come from the usual scripts/lib/qnx-target.sh variables
# plus QNX_SSH_KEY / QNX_SSH_KNOWN_HOSTS (optional).
set -euo pipefail
dir="$1"; shift
opts=(-o BatchMode=yes -o ConnectTimeout=10)
[[ -n "${QNX_SSH_KEY:-}" ]] && opts+=(-i "$QNX_SSH_KEY")
[[ -n "${QNX_SSH_KNOWN_HOSTS:-}" ]] && opts+=(-o "UserKnownHostsFile=$QNX_SSH_KNOWN_HOSTS")
target="${QNX_TARGET_USER:-qnxuser}@${QNX_TARGET_IP:-qnxpi.local}"
ssh "${opts[@]}" "$target" "mkdir -p '$dir'"
scp -q "${opts[@]}" "$@" "$target:$dir/"
