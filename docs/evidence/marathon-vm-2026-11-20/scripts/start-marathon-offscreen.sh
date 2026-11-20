#!/bin/sh
# Starts the Marathon-Shell compositor (offscreen QPA, software Qt Quick) in the Alpine VM.
# Idempotent: stops only the processes it started earlier (pidfile), then starts fresh.
export XDG_RUNTIME_DIR=/tmp/xdg
mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"
OUT=$HOME/ms-evidence
BIN=$HOME/ms-build/shell/marathon-shell-bin
if [ -f $OUT/marathon.pid ]; then
  kill $(cat $OUT/marathon.pid) 2>/dev/null; sleep 1
fi
if [ -f $OUT/dbus.pid ]; then kill $(cat $OUT/dbus.pid) 2>/dev/null; fi
rm -f "$XDG_RUNTIME_DIR"/marathon-wayland-0 "$XDG_RUNTIME_DIR"/marathon-wayland-0.lock
# private session bus
rm -f $XDG_RUNTIME_DIR/bus
dbus-daemon --session --fork --address=unix:path=$XDG_RUNTIME_DIR/bus --print-pid=4 4>$OUT/dbus.pid
export DBUS_SESSION_BUS_ADDRESS=unix:path=$XDG_RUNTIME_DIR/bus
export QT_QPA_PLATFORM=offscreen
export QML_IMPORT_PATH=$HOME/ms-keel/qml:$HOME/Marathon-Shell/marathon-silica
export QML2_IMPORT_PATH=$QML_IMPORT_PATH
export QT_QUICK_BACKEND=software
export QT_QPA_OFFSCREEN_NO_GLX=1
export MARATHON_WL_SOCKET_NAME=marathon-wayland-0
export MARATHON_SCREENSHOT_PATH=$OUT/marathon-shot.png
export QT_LOGGING_RULES="*.debug=false"
cd $HOME/ms-build
setsid nohup $BIN --skip-lock > $OUT/marathon.log 2>&1 < /dev/null &
echo $! > $OUT/marathon.pid
i=0; while [ $i -lt 60 ] && [ ! -S $XDG_RUNTIME_DIR/marathon-wayland-0 ]; do sleep 1; i=$((i+1)); done
ls -la $XDG_RUNTIME_DIR/
