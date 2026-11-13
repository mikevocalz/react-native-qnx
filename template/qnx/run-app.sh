#!/bin/sh
# Launcher for __APP_ID__ on a QNX board. Run it by hand, from the desktop tile
# or from `rnqnx project dev` / `project run`; it is not part of boot.
#
# One instance per app. The app guards itself over a local socket, and this
# script guards the cases where the socket cannot (a stale socket, a process
# owned by another login): it looks for the app's process with pidin before it
# starts anything.
#
#   run-app.sh                 show the running app, or start it
#   run-app.sh --replace       stop the running app (quit, then SIGTERM), then start
#   run-app.sh --dev-server U  as above, loading the bundle from Metro (dev client)
#
# Never SIGKILL: a process that ignores SIGTERM for 5 s makes this script fail
# instead of starting a second copy.
#
# Environment: RNQNX_APP_DIR (default: this script's directory) holds the app
# executable; RNQNX_QT_ROOT is the Qt runtime; RNQNX_LOG the log file;
# RNQNX_PIDIN the pidin command.

APP_ID=__APP_ID__
HERE=$(cd "$(dirname "$0")" && pwd)
APP_DIR=${RNQNX_APP_DIR:-$HERE}
QT_ROOT=${RNQNX_QT_ROOT:-/data/home/qnxuser/qt612-public-o2}
LOG=${RNQNX_LOG:-/tmp/rnqnx-$APP_ID.log}
PIDIN=${RNQNX_PIDIN:-pidin}
BIN=$APP_DIR/$APP_ID
TERM_WAIT=${RNQNX_TERM_WAIT:-5}

# The instance key: the app id without build-variant suffixes. Keep this list
# in step with buildVariantSuffixes() in native/window-chrome/WindowHost.cpp.
base_id() {
  id=$1
  while :; do
    case $id in
      *.*.dev | *.*.debug | *.*.preview) id=${id%.*} ;;
      *) break ;;
    esac
  done
  echo "$id"
}
BASE=$(base_id "$APP_ID")

# Pids of this app's processes, in any build variant: this user's, or every
# user's when run as root (root sees them all and may stop them).
running_pids() {
  me=${RNQNX_UID:-$(id -u)}
  "$PIDIN" -F '%a %N %U' 2>/dev/null | while read -r pid name uid rest; do
    case $pid in '' | *[!0-9]*) continue ;; esac
    [ "$me" = 0 ] || [ "$uid" = "$me" ] || continue
    name=${name##*/}
    [ "$(base_id "$name")" = "$BASE" ] || continue
    [ "$pid" = "$$" ] && continue
    echo "$pid"
  done
}

# SIGTERM every running copy and wait up to TERM_WAIT seconds for them to go.
terminate_running() {
  pids=$(running_pids)
  [ -n "$pids" ] || return 0
  # shellcheck disable=SC2086
  kill $pids 2>/dev/null
  n=0
  while [ -n "$(running_pids)" ]; do
    n=$((n + 1))
    if [ "$n" -gt "$TERM_WAIT" ]; then
      echo "rnqnx: $APP_ID (pid $(running_pids | tr '\n' ' ')) did not stop after SIGTERM; not starting a second copy" >&2
      return 1
    fi
    sleep 1
  done
  return 0
}

# The app runs below and, to restore or stop a running copy, above.
export LD_LIBRARY_PATH="$APP_DIR:$QT_ROOT/lib:/data/pi400-graphics-compat/lib/dll:/data/pi400-graphics-compat/usr/lib:/data/pi400-graphics-compat/usr/lib/graphics/rpi4-drm:/usr/lib"
# no-fullscreen: the window opens at the app size, a real window under
# rnqnx-winmgr.
export QT_QPA_PLATFORM=qnx:no-fullscreen
export QT_QPA_PLATFORM_PLUGIN_PATH=$QT_ROOT/plugins/platforms
# The app's own plugins (TLS backend, image formats) first, then the runtime's.
export QT_PLUGIN_PATH=$APP_DIR/plugins:$QT_ROOT/plugins
export QML_IMPORT_PATH=$QT_ROOT/qml
export QML2_IMPORT_PATH=$QT_ROOT/qml
export QT_QPA_FONTDIR=/usr/share/fonts/dejavu
export QT_FORCE_STDERR_LOGGING=1
unset QT_QUICK_BACKEND
unset DL_DEBUG

replace=
for arg in "$@"; do
  [ "$arg" = --replace ] && replace=1
done

if [ -n "$(running_pids)" ]; then
  if [ -n "$replace" ]; then
    # Ask the app to quit (graceful close), then make sure nothing is left.
    "$BIN" --stop-running >/dev/null 2>&1
    terminate_running || exit 1
  else
    # Restore the running window. When the app cannot be reached (stale
    # socket, a different login) it exits 75 without starting; stop the old
    # process and start fresh, so there is still exactly one.
    "$BIN" --restore-only "$@"
    status=$?
    [ "$status" -eq 0 ] && exit 0
    if [ "$status" -ne 75 ]; then exit "$status"; fi
    terminate_running || exit 1
  fi
fi

[ -t 1 ] || exec >>"$LOG" 2>&1
cd "$APP_DIR" || exit 1
exec "$BIN" "$@"
