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
# After each injected event it waits for the app to log the result (a new
# RNQNX_WINDOW_FRAME or RNQNX_WINDOW_ACTION line) for up to RNQNX_MOUSE_WAIT_S
# seconds (default 10) and names the line it waited for when none arrives.
# Evidence goes to artifacts/pi-mouse-smoke/<time>/.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"
# shellcheck source=scripts/lib/qnx-wait.sh
source "${SCRIPT_DIR}/lib/qnx-wait.sh"
rnqnx_target_init

APP_LOG="${RNQNX_APP_LOG:-/tmp/rnqnx-showcase.log}"
REMOTE_BIN="${RNQNX_INJECT_REMOTE:-/data/home/qnxuser/bin/rnqnx-inject}"
OUT="${RNQNX_MOUSE_EVIDENCE_DIR:-${PROJECT_ROOT}/artifacts/pi-mouse-smoke/$(date -u +%Y%m%dT%H%M%SZ)}"
BIN="${PROJECT_ROOT}/build/qnx-input/rnqnx-inject"
WAIT_S="${RNQNX_MOUSE_WAIT_S:-10}"
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
# Count of lines the app has logged for one marker.
# grep -c exits 1 when nothing matches, so the remote command ends in `|| true`
# and a failed ssh (empty output) stays an error instead of reading as zero.
logged() {
  local n
  n=$(rnqnx_ssh "grep -a -c '$1' '${APP_LOG}' || true") || return 1
  [[ "${n}" =~ ^[0-9]+$ ]] || { echo "ERROR: cannot count $1 lines in ${APP_LOG} (got '${n}')" >&2; return 1; }
  echo "${n}"
}
# wait_logged <marker> <count> <what>: block until the log holds <count> lines
# of <marker>. The error text from the wait goes to the result file.
wait_logged() {
  local err
  if err=$(rnqnx_wait_log_remote "${APP_LOG}" "^$1( |\$)" "$2" "${WAIT_S}" "$3" 2>&1); then
    return 0
  fi
  log "${err}"
  return 1
}
fails=0
log() { echo "$*" | tee -a "${OUT}/result.txt"; }

# expect <name> <width> <height> <x> <y> <inject args...>
# The window manager rounds a position by a pixel now and then, so size must
# match exactly and the position within 2 px.
expect() {
  local name=$1 ew=$2 eh=$3 ex=$4 ey=$5
  shift 5
  local before
  before=$(logged RNQNX_WINDOW_FRAME)
  inject "$@"
  local got=''
  if wait_logged RNQNX_WINDOW_FRAME $((before + 1)) "a new RNQNX_WINDOW_FRAME line after '${name}'"; then
    got=$(last_frame)
  fi
  if [[ "${got}" =~ ^([0-9]+)x([0-9]+)\+([0-9]+)\+([0-9]+)$ ]] && ((BASH_REMATCH[1] == ew && BASH_REMATCH[2] == eh)) &&
    ((BASH_REMATCH[3] - ex <= 2 && ex - BASH_REMATCH[3] <= 2 && BASH_REMATCH[4] - ey <= 2 && ey - BASH_REMATCH[4] <= 2)); then
    log "PASS ${name}: ${got}"
    x=${BASH_REMATCH[3]} y=${BASH_REMATCH[4]} w=${BASH_REMATCH[1]}
    bar=$((y + 22))
  else
    log "FAIL ${name}: got '${got}', want ${ew}x${eh}+${ex}+${ey}"
    fails=$((fails + 1))
  fi
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
expect "title-bar drag" "${w}" "${h}" $((x + dx)) $((y + dy)) drag $((x + w / 2)) ${bar} $((x + w / 2 + dx)) $((bar + dy)) 10
# right edge resize: 100 px narrower
expect "right edge resize" $((w - 100)) "${h}" "${x}" "${y}" drag $((x + w - 3)) $((y + h / 2)) $((x + w - 103)) $((y + h / 2)) 10
# double click on the bar maximises: the app logs an ACTION line each time, so
# count them. The second double click restores the frame.
actions() { logged RNQNX_WINDOW_ACTION; }
before=$(actions)
inject dclick $((x + w / 2)) ${bar}
wait_logged RNQNX_WINDOW_ACTION $((before + 1)) "RNQNX_WINDOW_ACTION line after the maximise double click" || true
if (($(actions) == before + 1)); then log "PASS double-click maximise: one RNQNX_WINDOW_ACTION logged"; else log "FAIL double-click maximise: expected one new RNQNX_WINDOW_ACTION"; fails=$((fails + 1)); fi
inject dclick 960 22
wait_logged RNQNX_WINDOW_ACTION $((before + 2)) "RNQNX_WINDOW_ACTION line after the restore double click" || true
if (($(actions) == before + 2)); then log "PASS double-click restore: second RNQNX_WINDOW_ACTION logged"; else log "FAIL double-click restore: expected two new RNQNX_WINDOW_ACTION lines"; fails=$((fails + 1)); fi
if ((fails == 0)); then log "mouse smoke: all checks passed"; else log "mouse smoke: ${fails} failed"; fi
exit $((fails != 0))
