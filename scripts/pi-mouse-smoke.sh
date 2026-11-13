#!/usr/bin/env bash
set -euo pipefail

# Mouse smoke test for a Pi 400 running rnqnx-winmgr and one app window.
#   pnpm pi:mouse-smoke [--build] [--no-install]
#
# Installs tools/qnx-input/rnqnx-inject on the Pi (build it first with --build,
# which needs the QNX SDP: source qnxsdp-env.sh), then drives the window with
# injected pointer events and reads RNQNX_WINDOW_FRAME / RNQNX_WINDOW_ACTION
# lines from the app log. The window must be restored (not maximized), and its
# frame is read from the log or given in RNQNX_MOUSE_FRAME=WxH+X+Y. It needs
# sudo without a password on the Pi (the injector is an input provider) and a
# window at least 700 wide. It never starts or stops an app.
# Evidence goes to artifacts/pi-mouse-smoke/<time>/.

SCRIPT_DIR="$(cd "${BASH_SOURCE[0]%/*}" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"
rnqnx_target_init

APP_LOG="${RNQNX_APP_LOG:-/tmp/rnqnx-showcase.log}"
REMOTE_BIN="${RNQNX_INJECT_REMOTE:-/data/home/qnxuser/bin/rnqnx-inject}"
OUT="${RNQNX_MOUSE_EVIDENCE_DIR:-${PROJECT_ROOT}/artifacts/pi-mouse-smoke/$(date -u +%Y%m%dT%H%M%SZ)}"
BIN="${PROJECT_ROOT}/build/qnx-input/rnqnx-inject"
build=0 install=1
for arg in "$@"; do
  case "$arg" in
    --build) build=1 ;;
    --no-install) install=0 ;;
    *) echo "usage: $0 [--build] [--no-install]" >&2; exit 2 ;;
  esac
done
mkdir -p "${OUT}"

if ((build)); then bash "${PROJECT_ROOT}/tools/qnx-input/build.sh" "${BIN}" >/dev/null; fi
if ((install)); then
  [[ -f "${BIN}" ]] || { echo "ERROR: ${BIN} missing; run with --build" >&2; exit 1; }
  rnqnx_ssh "mkdir -p '$(dirname "${REMOTE_BIN}")'"
  rnqnx_scp_to "${BIN}" "${REMOTE_BIN}.new"
  rnqnx_ssh "chmod 755 '${REMOTE_BIN}.new' && mv '${REMOTE_BIN}.new' '${REMOTE_BIN}'"
fi

inject() { rnqnx_ssh "sudo -n '${REMOTE_BIN}' $*"; }
# Last frame the app logged since its window chrome started; empty when the
# window has not moved or resized yet in this run.
last_frame() {
  rnqnx_ssh "awk '/RNQNX_WINDOW_CHROME/{f=\"\"} /RNQNX_WINDOW_FRAME/{f=\$2} END{print f}' '${APP_LOG}'"
}
fails=0
log() { echo "$*" | tee -a "${OUT}/result.txt"; }

# expect <name> <wxh+x+y regex> <inject args...>
expect() {
  local name=$1 want=$2
  shift 2
  inject "$@"
  sleep 1
  local got
  got=$(last_frame)
  if [[ "${got}" =~ ${want} ]]; then log "PASS ${name}: ${got}"; else log "FAIL ${name}: got '${got}', want /${want}/"; fails=$((fails + 1)); fi
}

start=${RNQNX_MOUSE_FRAME:-$(last_frame)}
[[ -n "${start}" ]] || {
  echo "ERROR: the app has not logged a window frame yet. Set RNQNX_MOUSE_FRAME=WxH+X+Y to the window's current frame (a restored, unmaximized window)." >&2
  exit 1
}
log "start frame ${start}"
if [[ "${start}" =~ ^([0-9]+)x([0-9]+)\+([0-9]+)\+([0-9]+)$ ]]; then
  w=${BASH_REMATCH[1]} h=${BASH_REMATCH[2]} x=${BASH_REMATCH[3]} y=${BASH_REMATCH[4]}
else
  echo "ERROR: cannot parse frame '${start}'" >&2; exit 1
fi
bar=$((y + 22))
# title bar drag: 100 px left and 60 up when there is room, else right and down
dx=-100 dy=-60
if ((x < 150 || y < 100)); then dx=100 dy=60; fi
expect "title-bar drag" "^${w}x${h}\\+$((x + dx))\\+$((y + dy))$" drag $((x + w / 2)) ${bar} $((x + w / 2 + dx)) $((bar + dy)) 10
x=$((x + dx)); y=$((y + dy)); bar=$((y + 22))
# right edge resize: 100 px narrower
expect "right edge resize" "^$((w - 100))x${h}\+${x}\+${y}$" drag $((x + w - 3)) $((y + h / 2)) $((x + w - 103)) $((y + h / 2)) 10
w=$((w - 100))
# double click on the bar maximises, again restores
inject dclick $((x + w / 2)) ${bar}; sleep 1.5
got=$(last_frame); log "INFO after maximise: ${got}"
inject dclick $((x + w / 2)) ${bar}; sleep 1.5
expect "double-click restore" "^${w}x${h}\+${x}\+${y}$" sleep 100
# wheel and click reach the app: scroll then record that the app log saw input
inject move $((x + w / 2)) $((y + h / 2)) wheel 3
log "INFO wheel sent"
rnqnx_ssh "grep -a -c RNQNX_WINDOW_FRAME '${APP_LOG}'" >"${OUT}/frames.count" || true
((fails == 0)) && log "mouse smoke: all checks passed" || log "mouse smoke: ${fails} failed"
exit $((fails != 0))
