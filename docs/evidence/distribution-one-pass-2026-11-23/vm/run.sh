#!/bin/sh
# QNX VM (QEMU, TCG, software render). usage: run.sh <label> <plugin dir>   (plugin dir holds platforms/libqqnx.so)
# LD_LIBRARY_PATH holds only QNX Screen libraries, VM system libraries and the Hermes lib, never the Qt directory:
# the app finds Qt through its DT_RUNPATH /data/qt612-public/lib.
label=$1; plug=$2
d=/data/dist3; vm=/data/vm
export LD_LIBRARY_PATH=$d/sys:$d/app:$vm/screen/lib:$vm/screen/lib/dll:$vm/screen/gfx:/system/lib:/proc/boot
export QT_QPA_PLATFORM=qnx:no-fullscreen QT_QUICK_BACKEND=software
export QT_QPA_PLATFORM_PLUGIN_PATH=$plug/platforms QT_PLUGIN_PATH=$plug:/data/qt612-public/plugins
export QML_IMPORT_PATH=/data/qt612-public/qml QML2_IMPORT_PATH=/data/qt612-public/qml QT_FORCE_STDERR_LOGGING=1
export QT_QPA_FONTDIR=$vm/fonts
if [ ! -e /dev/screen/command ]; then
  LD_PRELOAD=libm.so.3 on -d $vm/screen/sbin/screen -c $vm/screen/gfx/graphics.conf </dev/null >/tmp/screen.log 2>&1
  waitfor /dev/screen/command 15 || { echo SCREEN_NOT_UP; exit 1; }
fi
mkdir -p $d/out; cd $d/app
DL_DEBUG=libs ./rnqnx_playground_qnx --fullscreen --bundle $d/bundles/playground.hbc --screenshot $d/out/$label.png >$d/out/$label.log 2>&1
echo "$label exit=$? png=$(ls -l $d/out/$label.png 2>/dev/null | awk '{print $5}')"
