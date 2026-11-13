#!/usr/bin/env bash
# Deploys rnqnx_webgpu_test to a NEW directory on the Pi, runs the device-side
# launcher, and copies the run directory back.
#   scripts/webgpu/run-on-pi.sh <rnqnx_webgpu_test binary> <local-evidence-dir> [-- test args]
# Environment (same as deploy-bin.sh): QNX_TARGET_IP, QNX_TARGET_USER,
# QNX_SSH_KEY, QNX_SSH_KNOWN_HOSTS. REMOTE_BASE defaults to
# /data/home/<user>/rnqnx-webgpu.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
binary="${1:?usage: run-on-pi.sh <binary> <evidence-dir> [-- args]}"
evidence="${2:?}"
shift 2
[[ "${1:-}" == "--" ]] && shift

user="${QNX_TARGET_USER:-qnxuser}"
remote="${REMOTE_BASE:-/data/home/$user/rnqnx-webgpu}/$(date +%Y%m%dT%H%M%S)"
opts=(-o BatchMode=yes -o ConnectTimeout=10)
[[ -n "${QNX_SSH_KEY:-}" ]] && opts+=(-i "$QNX_SSH_KEY")
[[ -n "${QNX_SSH_KNOWN_HOSTS:-}" ]] && opts+=(-o "UserKnownHostsFile=$QNX_SSH_KNOWN_HOSTS")
target="$user@${QNX_TARGET_IP:-qnxpi.local}"

"$here/deploy-bin.sh" "$remote" "$binary" "$here/rnqnx-webgpu-launch.sh"
ssh "${opts[@]}" "$target" "chmod +x '$remote/rnqnx_webgpu_test' '$remote/rnqnx-webgpu-launch.sh'"
# Quote each argument for the remote shell.
args=""
for a in "$@"; do args+=" $(printf '%q' "$a")"; done
status=0
ssh "${opts[@]}" "$target" "cd '$remote' && ./rnqnx-webgpu-launch.sh$args" || status=$?
mkdir -p "$evidence"
scp -q -r "${opts[@]}" "$target:$remote/run-*" "$evidence/" || echo "warning: could not copy the run directory back" >&2
echo "evidence copied to $evidence (remote: $remote), launcher exit status: $status"
exit "$status"
