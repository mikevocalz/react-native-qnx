#!/bin/sh
# The host connects straight to Marathon-Shell's socket (no launch through the
# shell): nitro proof, screenshot, WAYLAND_DEBUG client log, repo log checker.
cd ~/ms-evidence; mkdir -p run
export XDG_RUNTIME_DIR=/tmp/xdg WAYLAND_DISPLAY=marathon-wayland-0 DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/xdg/bus QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=software
WAYLAND_DEBUG=client timeout 120 ~/rnq-build/host/bin/rnqnx_marathon_host --nitro-proof --screenshot run/direct-client.png > run/direct-host.log 2> run/direct-client.log
echo host_rc=$?
cat run/direct-host.log
python3 ~/rnq/tests/native/marathon-wayland/check-wayland-client-log.py run/direct-client.log
grep -E "xdg_toplevel#[0-9]+\.configure" run/direct-client.log | head -3 | cut -c1-120
