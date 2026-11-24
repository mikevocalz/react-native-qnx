#!/bin/sh
# Hand launcher for the 2026-11-24 receipts. Not part of boot. One instance per app:
# refuses to start while a process of the same name runs.
# usage: v1g2-run.sh <app-dir-name> <binary-name> [app args...]   (under /data/home/qnxuser/v1g2)
QT=${RNQNX_QT_ROOT:-/data/qt612-public}
D=/data/home/qnxuser/v1g2/$1; BIN=$2; shift 2
K=$BIN; L=${#K}; [ $L -gt 15 ] && K=$(echo "$K" | cut -c$((L-14))-$L)
case " $* " in *" --restore-only "*|*" --stop-running "*) SKIPCHECK=1;; esac
if [ -z "$SKIPCHECK" ] && sudo -n pidin -F '%a %N' | grep -qF "$K"; then echo "ALREADY RUNNING $BIN" >&2; exit 3; fi
export LD_LIBRARY_PATH="$D:$QT/lib:/data/pi400-graphics-compat/lib/dll:/data/pi400-graphics-compat/usr/lib:/data/pi400-graphics-compat/usr/lib/graphics/rpi4-drm:/usr/lib"
export QT_QPA_PLATFORM=${RNQNX_QPA:-qnx:no-fullscreen}
export QT_QPA_PLATFORM_PLUGIN_PATH=$QT/plugins/platforms
export QT_PLUGIN_PATH=$D/plugins:$QT/plugins
export QML_IMPORT_PATH=$QT/qml QML2_IMPORT_PATH=$QT/qml
export QT_QPA_FONTDIR=/usr/share/fonts/dejavu
export QT_FORCE_STDERR_LOGGING=1
unset QT_QUICK_BACKEND DL_DEBUG
cd $D && exec ./$BIN "$@"
