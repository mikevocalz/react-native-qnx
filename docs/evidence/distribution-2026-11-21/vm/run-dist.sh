#!/bin/sh
# QEMU VM. usage: run-dist.sh <label> <extra LD_LIBRARY_PATH prefix or -> 
# Runs the showcase app from /data/dist-test with LD_LIBRARY_PATH holding only the
# QNX Screen libraries (and the optional prefix), never the Qt directory.
label=$1; pre=$2
d=/data/dist-test; vm=/data/vm
[ "$pre" = "-" ] && pre=
export LD_LIBRARY_PATH=${pre:+$pre:}/data/dist-test/sys:$vm/screen/lib:$vm/screen/lib/dll:$vm/screen/gfx:/system/lib:/proc/boot
export QT_QPA_PLATFORM=qnx:no-fullscreen QT_QUICK_BACKEND=software
export QT_QPA_PLATFORM_PLUGIN_PATH=$d/qt/plugins/platforms QT_PLUGIN_PATH=$d/qt/plugins
export QML_IMPORT_PATH=$d/qt/qml QML2_IMPORT_PATH=$d/qt/qml QT_FORCE_STDERR_LOGGING=1
export QT_QPA_FONTDIR=$vm/fonts RNQNX_WINDOW_MANAGER=fullscreen
if [ ! -e /dev/screen/command ]; then
  LD_PRELOAD=libm.so.3 on -d $vm/screen/sbin/screen -c $vm/screen/gfx/graphics.conf </dev/null >/tmp/screen.log 2>&1
  waitfor /dev/screen/command 15 || { echo SCREEN_NOT_UP; exit 1; }
fi
mkdir -p $d/out; cd $d/app
DL_DEBUG=libs ./org.rnqnx.showcase --fullscreen --bundle $d/bundles/proof-button.hbc --screenshot $d/out/$label.png >$d/out/$label.log 2>&1
echo "$label exit=$? png=$(ls -l $d/out/$label.png 2>/dev/null | awk '{print $5}')"
