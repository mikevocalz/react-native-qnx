#!/bin/sh
# Runs on the QEMU VM. Starts Screen (software blitter, dummy WFD display) if it
# is not running, then runs the ADR-0021 JS-thread test against it.
#
#   js-thread.sh <test-dir> <out-dir> [test arguments...]
#
# <test-dir> holds qt_react_host_js_thread_qnx_test and a scripts/ directory of
# hermesc bytecode (one .hbc per file in tests/fixtures/js-thread). The window
# grabs the test takes mid-block go to <out-dir>/shots. The test's stdout and
# stderr go to <out-dir>/run.log.
#
# Output from this script is QEMU VM evidence (TCG, software render). It is not
# Pi 400 evidence and its timings are not performance evidence.
set -u
dir=$1; out=$2; shift 2
vm=/data/vm
export LD_LIBRARY_PATH=$dir:$vm/qt/lib:$vm/screen/lib:$vm/screen/lib/dll:$vm/screen/gfx:/system/lib:/proc/boot
export QT_QPA_PLATFORM=qnx:no-fullscreen
export QT_QUICK_BACKEND=software
export QT_QPA_PLATFORM_PLUGIN_PATH=$vm/qt/plugins/platforms
export QT_PLUGIN_PATH=$vm/qt/plugins
export QML_IMPORT_PATH=$vm/qt/qml
export QML2_IMPORT_PATH=$vm/qt/qml
export QT_FORCE_STDERR_LOGGING=1
export QT_QPA_FONTDIR=$vm/fonts
export RNQNX_WINDOW_MANAGER=fullscreen
export RNQNX_JS_THREAD_SCRIPTS=$dir/scripts
export RNQNX_JS_THREAD_SHOTS=$out/shots
mkdir -p "$out/shots"
if [ ! -e /dev/screen/command ]; then
  # libyuv.so uses log10 but does not list libm as a dependency; preload it.
  LD_PRELOAD=libm.so.3 on -d $vm/screen/sbin/screen -c $vm/screen/gfx/graphics.conf </dev/null >/tmp/screen.log 2>&1
  waitfor /dev/screen/command 15 || { echo SCREEN_NOT_UP; exit 1; }
fi
cd "$dir" || exit 1
rc=0
./qt_react_host_js_thread_qnx_test "$@" >"$out/run.log" 2>&1 || rc=$?
echo "exit=$rc"
cat "$out/run.log"
