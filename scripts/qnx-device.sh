#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"

fail() { echo "qnx-device: $*" >&2; exit 2; }
[[ $# -ge 1 ]] || fail 'Missing device command.'
command="$1"
shift
# No caller-supplied arbitrary OpenSSH options or config-file commands/forwards.
[[ -z "${QNX_SSH_OPTS:-}" ]] || fail 'QNX_SSH_OPTS is unsupported by restricted device tools.'
[[ "${QNX_SSH_PORT:-22}" =~ ^[1-9][0-9]{0,4}$ ]] || fail 'Invalid QNX_SSH_PORT.'
[[ "${QNX_SSH_CONNECT_TIMEOUT:-10}" =~ ^[1-9][0-9]{0,2}$ ]] || fail 'Invalid SSH timeout.'
(( ${QNX_SSH_CONNECT_TIMEOUT:-10} <= 120 )) || fail 'SSH timeout must be at most 120 seconds.'
rnqnx_target_init
# Explicit flags take precedence over shared optional settings. Known-host
# verification is never disabled; an agent may supply the authentication key.
secure=(-F none -T -a -x -S none -o BatchMode=yes -o StrictHostKeyChecking=yes
        -o ControlMaster=no -o ForkAfterAuthentication=no -o PermitLocalCommand=no)
address="$(rnqnx_target_address)"
case "$command" in
  status)
    [[ $# -eq 0 ]] || fail 'status takes no arguments.'
    exec ssh "${secure[@]}" "${RNQNX_SSH_ARGS[@]}" "$address" 'sh -s' <<'REMOTE'
set -eu
os=$(uname -s)
if [ "$os" != QNX ]; then
  echo 'Expected QNX target; no platform readiness result.' >&2
  exit 42
fi
architecture=$(uname -m)
printf 'os=%s\narchitecture=%s\n' "$os" "$architecture"
for tool in slog2info screenshot; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf '%s=available\n' "$tool"
  else
    printf '%s=missing\n' "$tool"
  fi
done
REMOTE
    ;;
  logs|logs-follow)
    [[ $# -eq 0 ]] || fail 'logs takes no positional arguments.'
    remote='slog2info'
    [[ "$command" != logs-follow ]] || remote='slog2info -w'
    exec ssh "${secure[@]}" "${RNQNX_SSH_ARGS[@]}" "$address" "$remote"
    ;;
  port-forward)
    [[ $# -eq 2 ]] || fail 'Two port numbers are required.'
    [[ "$1" =~ ^[1-9][0-9]{0,4}$ && "$2" =~ ^[1-9][0-9]{0,4}$ ]] || fail 'Invalid port.'
    (( $1 >= 1024 && $1 <= 65535 && $2 >= 1 && $2 <= 65535 )) || fail 'Port out of range.'
    exec ssh "${secure[@]}" -n -N -o ExitOnForwardFailure=yes -o GatewayPorts=no \
      -L "127.0.0.1:$1:127.0.0.1:$2" "${RNQNX_SSH_ARGS[@]}" "$address"
    ;;
  screenshot)
    [[ $# -eq 1 && "$1" =~ ^[a-f0-9]{24}$ ]] || fail 'Invalid screenshot capture identity.'
    exec ssh "${secure[@]}" "${RNQNX_SSH_ARGS[@]}" "$address" \
      "sh -s -- '${QNX_TARGET_DIR}' '$1'" <<'REMOTE'
set -eu
command -v screenshot >/dev/null 2>&1 || { echo 'screenshot is not installed.' >&2; exit 127; }
dir="$1/.rnqnx-capture-$2"
umask 077
mkdir "$dir" || exit 40
cleanup() { rm -f "$dir/capture.bmp"; rmdir "$dir"; }
trap cleanup 0
trap 'exit 130' 1 2 15
screenshot -file="$dir/capture.bmp" >&2
test -s "$dir/capture.bmp" || exit 41
cat "$dir/capture.bmp"
REMOTE
    ;;
  *) fail 'Unknown device command.' ;;
esac
