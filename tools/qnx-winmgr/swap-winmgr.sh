#!/bin/sh
# Swaps the running window manager for a new rnqnx-winmgr build on a Pi 400
# that already boots into rnqnx-winmgr (install-boot.sh), without a reboot.
# Run as root:
#   sudo sh swap-winmgr.sh            install the staged binary and restart
#   sudo sh swap-winmgr.sh --check    show what it would do; changes nothing
#   sudo sh swap-winmgr.sh --undo     put the backup binary back the same way
# Idempotent: with the staged binary already installed and one manager
# running it only says so.
#
# What it does, in order:
#   1. keeps /data/rnqnx-winmgr/rnqnx-winmgr as ...before-single-instance
#      (first run only) and installs the staged binary with cp + mv;
#   2. stops the watcher subshell that would start fullscreen-winmgr and a new
#      launcher the moment the manager exits (rnqnx-desktop.sh runs it);
#   3. SIGTERMs the running manager (never SIGKILL) and starts the new one
#      with `-s demolauncher,st,gtk`, logging to /tmp/rnqnx-winmgr.log, with a
#      watcher of its own that brings the stock desktop back if it stops;
#   4. restarts demolauncher and, when the tile app was running, relaunches it
#      through its guarded launcher (--replace), so the new manager sees its
#      window;
#   5. checks one manager, one launcher, one tile app and the mouse node, and
#      rolls back to the backup binary if the new manager is gone within 5 s.
# Windows of other apps that were open stay on screen unmanaged until those
# apps are launched again.
#
# Every path and command can be moved for a dry run on copies; see the
# RNQNX_* variables below and tests/js/qnx-swap-winmgr.test.mjs.

STAGED=${RNQNX_WM_STAGED:-/data/home/qnxuser/rnqnx-wm/rnqnx-winmgr}
DIR=${RNQNX_WM_DIR:-/data/rnqnx-winmgr}
WM=$DIR/rnqnx-winmgr
BACKUP=$WM.before-single-instance
LOG=${RNQNX_WM_LOG:-/tmp/rnqnx-winmgr.log}
DESKTOP_FILES=${RNQNX_DESKTOP_FILES:-/system/etc/desktop_files}
TILE_LAUNCHER=${RNQNX_TILE_LAUNCHER:-/data/home/qnxuser/run-chrome-app.sh}
TILE_PROGRAM=${RNQNX_TILE_PROGRAM:-org.rnqnx.showcase}
RUN_AS=${RNQNX_RUN_AS-sudo -n -u qnxuser}
IO_HID=${RNQNX_IO_HID:-/dev/io-hid/io-hid}
PROC=${RNQNX_PROC:-/proc}
GRACE=${RNQNX_WM_GRACE:-5}
STOCK_WM=${RNQNX_STOCK_WM:-fullscreen-winmgr}
LAUNCHER=${RNQNX_LAUNCHER:-demolauncher}
LAUNCHER_NAME=${LAUNCHER##*/}
STOCK_NAME=${STOCK_WM##*/}

# A dropped SSH session must not kill the script or the programs it starts.
trap '' HUP

say() { echo "swap-winmgr: $*"; }
fail() { echo "swap-winmgr: $*" >&2; exit 1; }

list_procs() {
  if [ -n "${RNQNX_PIDIN:-}" ]; then "$RNQNX_PIDIN"; else pidin -F '%a %N'; fi
}

# pids_of <program name>: pids whose process name is exactly that.
pids_of() {
  list_procs 2>/dev/null | while read -r pid name rest; do
    case $pid in '' | *[!0-9]*) continue ;; esac
    # pidin may print a path (usr/bin/demolauncher).
    name=${name##*/}
    [ "$name" = "$1" ] && echo "$pid"
  done | sort -u
}

count_of() { pids_of "$1" | wc -l | tr -d ' '; }

# Subshells that restart the stock desktop when the manager stops: the one
# rnqnx-desktop.sh forks at boot, and the one this script leaves behind.
watcher_pids() {
  list_procs 2>/dev/null | while read -r pid name rest; do
    case $pid in '' | *[!0-9]*) continue ;; esac
    [ "$pid" = "$$" ] && continue
    [ -r "$PROC/$pid/cmdline" ] || continue
    cmd=$(tr '\0' ' ' <"$PROC/$pid/cmdline" 2>/dev/null)
    case $cmd in
      *rnqnx-desktop.sh* | *rnqnx-wm-watcher.sh*) echo "$pid" ;;
    esac
  done
  # The watcher this script started earlier.
  if [ -f "$DIR/rnqnx-wm-watcher.pid" ]; then
    wp=$(cat "$DIR/rnqnx-wm-watcher.pid")
    case $wp in
      '' | *[!0-9]*) ;;
      *)
        # Only if that pid is still the watcher: a reused pid is not signalled.
        # (Where /proc cannot be read at all, as in the host test, the pid
        # file is trusted; root can read it on QNX.)
        if [ -r "$PROC/$wp/cmdline" ]; then
          case $(tr '\0' ' ' <"$PROC/$wp/cmdline" 2>/dev/null) in
            *rnqnx-wm-watcher.sh*) echo "$wp" ;;
          esac
        elif kill -0 "$wp" 2>/dev/null; then
          echo "$wp"
        fi
        ;;
    esac
  fi
}

# Plain SIGTERM to each pid, then wait up to GRACE seconds. Never SIGKILL.
terminate() {
  [ -n "$*" ] || return 0
  kill "$@" 2>/dev/null
  n=0
  while :; do
    alive=
    for p in "$@"; do kill -0 "$p" 2>/dev/null && alive=1; done
    [ -n "$alive" ] || return 0
    n=$((n + 1))
    [ "$n" -gt "$GRACE" ] && return 1
    sleep 1
  done
}

preflight() {
  [ -f "$SOURCE" ] || fail "missing $SOURCE"
  [ -x "$SOURCE" ] || fail "$SOURCE is not executable"
  [ -d "$DIR" ] || fail "missing $DIR (install-boot.sh creates it)"
  if [ -e "$IO_HID" ]; then say "mouse node $IO_HID present"; else
    say "warning: $IO_HID is missing; the mouse will not work until io-hid runs"
  fi
  say "window managers running: $(count_of rnqnx-winmgr) rnqnx-winmgr, $(count_of "$STOCK_NAME") $STOCK_WM"
  say "launchers running: $(count_of "$LAUNCHER_NAME"); tile app ($TILE_PROGRAM) running: $(count_of "$TILE_PROGRAM")"
  say "watchers: $(watcher_pids | tr '\n' ' ')"
}

# The watcher: if the manager it was given stops, bring the stock desktop back
# (as rnqnx-desktop.sh does), so the board always has a desktop.
write_watcher() {
  cat >"$DIR/rnqnx-wm-watcher.sh.new" <<'WATCHER'
#!/bin/sh
# rnqnx-wm-watcher.sh <wm pid> <log> <desktop files dir> <stock wm> <launcher> <stock name> <launcher name>
# Started by swap-winmgr.sh. Exits quietly when the manager is stopped on
# purpose (swap-winmgr.sh kills this watcher first).
while kill -0 "$1" 2>/dev/null; do sleep 5; done
echo 'rnqnx-winmgr stopped; starting '"$4" >>"$2"
"$4" -s demolauncher,st,gtk &
sleep 2
for p in $(${RNQNX_PIDIN:-pidin} -F '%a %N' | while read -r pid name rest; do [ "${name##*/}" = "$7" ] && echo "$pid"; done); do kill "$p"; done
"$5" -a "$3" &
WATCHER
  chmod 755 "$DIR/rnqnx-wm-watcher.sh.new" && mv -f "$DIR/rnqnx-wm-watcher.sh.new" "$DIR/rnqnx-wm-watcher.sh"
}

# start_wm <binary>: starts the manager; sets WMPID. The watcher starts only
# after the manager has stayed up (start_watcher), so it cannot race the check.
start_wm() {
  "$1" -s demolauncher,st,gtk </dev/null >>"$LOG" 2>&1 &
  WMPID=$!
}

start_watcher() {
  write_watcher
  sh "$DIR/rnqnx-wm-watcher.sh" "$WMPID" "$LOG" "$DESKTOP_FILES" "$STOCK_WM" "$LAUNCHER" "$STOCK_NAME" "$LAUNCHER_NAME" </dev/null >/dev/null 2>&1 &
  echo $! >"$DIR/rnqnx-wm-watcher.pid"
}

# True when WMPID is still running after GRACE seconds (checked each second).
stays_up() {
  n=0
  while [ "$n" -lt "$GRACE" ]; do
    kill -0 "$WMPID" 2>/dev/null || return 1
    sleep 1
    n=$((n + 1))
  done
  kill -0 "$WMPID" 2>/dev/null
}

restart_launcher() {
  # shellcheck disable=SC2046
  terminate $(pids_of "$LAUNCHER_NAME") || fail "$LAUNCHER did not stop after SIGTERM"
  "$LAUNCHER" -a "$DESKTOP_FILES" </dev/null >/dev/null 2>&1 &
}

relaunch_tile() {
  [ -n "$TILE_WAS_RUNNING" ] || return 0
  say "relaunching the tile app through $TILE_LAUNCHER --replace"
  # shellcheck disable=SC2086
  $RUN_AS "$TILE_LAUNCHER" --replace </dev/null >/dev/null 2>&1 &
  n=0
  while [ "$(count_of "$TILE_PROGRAM")" != 1 ] && [ "$n" -lt 30 ]; do
    n=$((n + 1))
    sleep 1
  done
}

verify() {
  ok=1
  [ "$(count_of rnqnx-winmgr)" = 1 ] || { say "FAIL: $(count_of rnqnx-winmgr) rnqnx-winmgr processes"; ok=; }
  [ "$(count_of "$STOCK_NAME")" = 0 ] || { say "FAIL: $STOCK_WM is running beside rnqnx-winmgr"; ok=; }
  [ "$(count_of "$LAUNCHER_NAME")" = 1 ] || { say "FAIL: $(count_of "$LAUNCHER_NAME") $LAUNCHER processes"; ok=; }
  if [ -n "$TILE_WAS_RUNNING" ]; then
    [ "$(count_of "$TILE_PROGRAM")" = 1 ] || { say "FAIL: $(count_of "$TILE_PROGRAM") $TILE_PROGRAM processes"; ok=; }
  fi
  [ -e "$IO_HID" ] || { say "FAIL: $IO_HID is missing"; ok=; }
  [ -n "$ok" ]
}

# install_and_restart <source> <label>
apply() {
  SOURCE=$1
  preflight
  if cmp -s "$SOURCE" "$WM" && [ "$(count_of rnqnx-winmgr)" = 1 ] && [ "$MODE" != undo ]; then
    say "already installed and running; nothing to do"
    exit 0
  fi
  if [ "$MODE" = apply ] && [ -f "$WM" ] && [ ! -f "$BACKUP" ]; then
    cp -p "$WM" "$BACKUP" || fail "cannot write $BACKUP"
    say "kept the current manager as $BACKUP"
  fi
  TILE_WAS_RUNNING=
  [ "$(count_of "$TILE_PROGRAM")" -gt 0 ] && TILE_WAS_RUNNING=1

  if ! { cp "$SOURCE" "$WM.new" && chmod 755 "$WM.new" && mv -f "$WM.new" "$WM"; }; then fail "cannot install $WM"; fi
  say "installed $SOURCE as $WM"

  # The watcher first, so the stock desktop does not start in between; the
  # stock manager too, when the board had fallen back to it.
  # shellcheck disable=SC2046
  terminate $(watcher_pids) || fail "a watcher did not stop; the new binary is installed, nothing is restarted"
  # shellcheck disable=SC2046
  terminate $(pids_of rnqnx-winmgr) $(pids_of "$STOCK_NAME") || fail "the running window manager did not stop after SIGTERM; the new binary is installed but not running (undo: sh $0 --undo)"
  start_wm "$WM"
  if ! stays_up; then
    say "the new manager exited within ${GRACE}s; rolling back"
    if [ -f "$BACKUP" ]; then
      cp -p "$BACKUP" "$WM.new" && mv -f "$WM.new" "$WM"
      say "restored $BACKUP"
      start_wm "$WM"
      start_watcher
    else
      say "no backup; starting $STOCK_WM"
      "$STOCK_WM" -s demolauncher,st,gtk </dev/null >>"$LOG" 2>&1 &
    fi
    sleep 2
    restart_launcher
    relaunch_tile
    fail "rolled back; the new manager's last log lines are in $LOG"
  fi
  start_watcher
  restart_launcher
  relaunch_tile
  sleep 2
  if verify; then
    say "done: one rnqnx-winmgr (pid $WMPID), one $LAUNCHER${TILE_WAS_RUNNING:+, one $TILE_PROGRAM}"
    exit 0
  fi
  fail "the checks above failed; the manager is running. Undo with: sh $0 --undo"
}

MODE=apply
case ${1:-} in
  '') ;;
  --check) MODE=check ;;
  --undo) MODE=undo ;;
  *) fail "usage: sh $0 [--check|--undo]" ;;
esac

if [ "$MODE" = check ]; then
  SOURCE=$STAGED
  preflight
  if cmp -s "$SOURCE" "$WM" 2>/dev/null; then say "the staged binary is already installed"; else say "would install $SOURCE as $WM"; fi
  if [ -f "$BACKUP" ]; then say "backup exists: $BACKUP"; else say "would keep $WM as $BACKUP"; fi
  say "would stop watchers and the running rnqnx-winmgr (SIGTERM), start the new one, restart $LAUNCHER and relaunch $TILE_PROGRAM if running"
  exit 0
fi

[ "$(id -u)" = 0 ] || [ -n "${RNQNX_ALLOW_USER:-}" ] || fail "run as root: sudo sh $0"
if [ "$MODE" = undo ]; then
  [ -f "$BACKUP" ] || fail "no backup at $BACKUP"
  apply "$BACKUP"
fi
apply "$STAGED"
