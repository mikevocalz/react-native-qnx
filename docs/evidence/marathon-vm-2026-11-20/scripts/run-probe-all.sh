#!/bin/sh
# One full pass: fresh Marathon-Shell on the vnc QPA, host launched by the shell
# from its .desktop entry, then pointer, scroll, text, Back (handled), Back
# (unhandled), relaunch, exitApp. Output: run/*.png and run/host.log.
cd ~/ms-evidence || exit 1
export DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/xdg/bus
step() { printf '\n== %s %s\n' "$(date +%T)" "$*"; }
for p in $(ps | grep "[r]nqnx_marathon_host" | awk '{print $1}'); do kill $p 2>/dev/null; done
rm -f run/*.png; : > run/host.log; : > run/host.err
step "start shell (vnc QPA 720x1440, MARATHON_NATIVE_BACK_MS=4000)"
MS_BACK_MS=4000 MS_QPA="vnc:size=720x1440:port=5900" sh start-marathon-vnc.sh > /dev/null
sleep 6
step "wayland-info globals"
XDG_RUNTIME_DIR=/tmp/xdg WAYLAND_DISPLAY=marathon-wayland-0 wayland-info | grep -E "interface:" | sed 's/^ *//' | cut -c1-80 > run/globals.txt; wc -l < run/globals.txt
step "LaunchApp org.rnqnx.marathon.probe over D-Bus Navigation1"
dbus-send --session --print-reply --dest=org.marathonos.Shell /org/marathonos/Shell/Navigation org.marathonos.Shell.Navigation1.LaunchApp string:org.rnqnx.marathon.probe | tail -1
sleep 14
python3 rfb.py shot run/20-launched.png
step "pointer tap on Tap me"
python3 rfb.py tap 195 95 wait 1.5 shot run/21-tap.png
step "pointer drag scroll"
python3 rfb.py drag 195 600 195 300 wait 1.5 shot run/22-scroll.png
step "wheel (RFB buttons 4/5)"
python3 rfb.py wheel 195 400 down 5 wait 1.5 shot run/22b-wheel.png
step "focus text field and type"
python3 rfb.py tap 195 773 wait 1 type "hello marathon" wait 1.5 shot run/23-text.png
step "Back 1 (Escape reaches the shell, shell sends XF86Back): handled, window repaints"
python3 rfb.py key escape wait 2.5 shot run/24-back-handled.png
step "Back 2: handler returns false, nothing repaints, shell backs out after MARATHON_NATIVE_BACK_MS"
python3 rfb.py key escape wait 6 shot run/25-back-unhandled.png
step "host process still alive after back-out?"
ps | grep "[r]nqnx_marathon_host" | wc -l
step "relaunch while running (shell activates the running app)"
dbus-send --session --print-reply --dest=org.marathonos.Shell /org/marathonos/Shell/Navigation org.marathonos.Shell.Navigation1.LaunchApp string:org.rnqnx.marathon.probe | tail -1
sleep 5
python3 rfb.py shot run/26-relaunch.png
step "exitApp button"
python3 rfb.py tap 195 185 wait 3 shot run/27-after-exit.png
step "host process after exitApp"
ps | grep "[r]nqnx_marathon_host" | wc -l
step "done"
