#!/bin/sh
# Runs on the QEMU VM. Starts Screen (software blitter, dummy WFD display) if it
# is not running, then writes one PNG per proof bundle with the app's own
# --screenshot option.
#
#   capture.sh <app-dir> <bundle-dir> <out-dir> <card-id>...
#
# Evidence from this script is QEMU VM evidence. It is not Pi 400 evidence.
set -u
app=$1; bundles=$2; out=$3; shift 3
vm=/data/vm
export LD_LIBRARY_PATH=$app:$vm/qt/lib:$vm/screen/lib:$vm/screen/lib/dll:$vm/screen/gfx:/system/lib:/proc/boot
export QT_QPA_PLATFORM=qnx:no-fullscreen
export QT_QUICK_BACKEND=software
export QT_QPA_PLATFORM_PLUGIN_PATH=$vm/qt/plugins/platforms
export QT_PLUGIN_PATH=$vm/qt/plugins
export QML_IMPORT_PATH=$vm/qt/qml
export QML2_IMPORT_PATH=$vm/qt/qml
export QT_FORCE_STDERR_LOGGING=1
export QT_QPA_FONTDIR=$vm/fonts
export RNQNX_WINDOW_MANAGER=fullscreen
mkdir -p "$out"
if [ ! -e /dev/screen/command ]; then
  # libyuv.so uses log10 but does not list libm as a dependency; preload it.
  LD_PRELOAD=libm.so.3 on -d $vm/screen/sbin/screen -c $vm/screen/gfx/graphics.conf </dev/null >/tmp/screen.log 2>&1
  waitfor /dev/screen/command 15 || { echo SCREEN_NOT_UP; exit 1; }
fi
cd "$app" || exit 1
for id in "$@"; do
  rc=0
  ./org.rnqnx.showcase --fullscreen --bundle "$bundles/proof-$id.hbc" --screenshot "$out/$id.png" \
    >"$out/$id.log" 2>&1 || rc=$?
  echo "$id exit=$rc $(ls -l "$out/$id.png" 2>/dev/null | awk '{print $5}')"
done
