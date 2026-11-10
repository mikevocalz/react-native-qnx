#!/bin/sh
set -eu
umask 077
[ "$(uname -m)" = RaspberryPi400 ] || exit 0
root=/data/pi400-graphics-compat
ld="$root/lib/dll:$root/usr/lib:$root/usr/lib/graphics/rpi4-drm:/usr/lib:/lib:/lib/dll:/proc/boot"

case "${1-}" in
    --launcher-user)
        [ "$(id -u)" = 1000 ] || { echo PI400_DESKTOP_ERROR_LAUNCHER_UID; exit 1; }
        registry=/data/home/qnxuser/.local/share/rnqnx/launcher
        if [ -L "$registry" ] || [ -L "$registry/owner" ] || [ -L "$registry/desktop.conf" ] ||
           [ ! -f "$registry/owner" ] || [ ! -s "$registry/desktop.conf" ] ||
           [ "$(cat "$registry/owner")" != react-native-qnx/launcher-v1 ]; then
            echo PI400_DESKTOP_STOCK_REGISTRY_FALLBACK
            registry=/system/etc/desktop_files
        fi
        [ -s "$registry/desktop.conf" ] || { echo PI400_DESKTOP_ERROR_REGISTRY; exit 1; }
        export LD_LIBRARY_PATH="$ld"
        echo PI400_DESKTOP_LAUNCHER_UID_1000
        exec /system/bin/demolauncher -a "$registry"
        ;;
    '') ;;
    *) echo PI400_DESKTOP_ERROR_ARGUMENT; exit 1 ;;
esac

# The legacy enable marker now selects only the desktop, never an application.
[ -f "$root/enable-rnqnx-autostart" ] || exit 0
[ "$(id -u)" = 0 ] || { echo PI400_DESKTOP_ERROR_REQUIRES_DRIVER_ADMIN; exit 1; }
for file in "$root/sbin/drm-rpi4" /system/bin/screen /system/bin/fullscreen-winmgr /system/bin/demolauncher; do
    [ -x "$file" ] || { echo "PI400_DESKTOP_ERROR_MISSING_EXECUTABLE $file"; exit 1; }
done
for file in "$root/lib/dll/screen-rpi4drm.so" "$root/usr/lib/libdrm.so.1" "$root/usr/lib/graphics/rpi4-drm/graphics-pi400-hdmi2.conf"; do
    [ -r "$file" ] || { echo "PI400_DESKTOP_ERROR_MISSING_RUNTIME $file"; exit 1; }
done
lock=/dev/shmem/pi400-rnqnx-autostart.lock
if ! (set -C; : > "$lock"); then
    echo PI400_DESKTOP_ERROR_LOCK_UNAVAILABLE
    exit 1
fi
trap 'rm -f "$lock"' EXIT
trap 'exit 143' TERM
trap 'exit 130' INT

logdir="$root/desktop-logs"
if [ -L "$logdir" ] || { [ -e "$logdir" ] && [ ! -d "$logdir" ]; }; then
    echo PI400_DESKTOP_ERROR_LOG_DIRECTORY
    exit 1
fi
mkdir -p "$logdir"

process_running() {
    pidin ar | awk -v executable="$1" '$2 == executable {found=1} END {exit !found}'
}

wait_process() {
    remaining=10
    while [ "$remaining" -gt 0 ]; do
        if process_running "$1"; then return 0; fi
        remaining=$((remaining - 1))
        sleep 1
    done
    return 1
}

start_input() {
    if [ ! -e /dev/usb/io-usb-otg ]; then
        [ -x /proc/boot/usb_start.sh ] || return 1
        on -d /bin/sh /proc/boot/usb_start.sh </dev/null >>"$logdir/usb.log" 2>&1 || return 1
    fi
    waitfor /dev/usb/io-usb-otg 15 || return 1
    waitfor /dev/usb/devu-hcd-bcm2711-xhci.so 15 || return 1
    if [ ! -e /dev/io-hid/io-hid ]; then
        on -d /system/bin/io-hid -dusb </dev/null >>"$logdir/hid.log" 2>&1 || return 1
    fi
    waitfor /dev/io-hid/io-hid 15 || return 1
    chown 36 /dev/io-hid/io-hid || return 1
    echo PI400_DESKTOP_INPUT_READY
}

if ! start_input; then
    echo PI400_DESKTOP_WARNING_INPUT_UNAVAILABLE
fi
if [ ! -e /dev/drm/card0 ]; then
    on -d -e LD_LIBRARY_PATH="$ld" "$root/sbin/drm-rpi4" -s </dev/null >>"$logdir/drm.log" 2>&1
fi
waitfor /dev/drm/card0 15 || { echo PI400_DESKTOP_ERROR_DRM; exit 1; }
waitfor /dev/drm/renderD128 15 || { echo PI400_DESKTOP_ERROR_DRM_RENDER_NODE; exit 1; }
if [ ! -e /dev/screen/command ]; then
    on -d -e LD_LIBRARY_PATH="$ld" /system/bin/screen -c "$root/usr/lib/graphics/rpi4-drm/graphics-pi400-hdmi2.conf" </dev/null >>"$logdir/screen.log" 2>&1
fi
waitfor /dev/screen/command 15 || { echo PI400_DESKTOP_ERROR_SCREEN; exit 1; }
setfacl -m u:qnxuser:rw- /dev/drm/card0 /dev/drm/renderD128
if ! process_running /system/bin/fullscreen-winmgr; then
    on -d /system/bin/fullscreen-winmgr -s demolauncher,st,gtk </dev/null >>"$logdir/winmgr.log" 2>&1
fi
wait_process /system/bin/fullscreen-winmgr || { echo PI400_DESKTOP_ERROR_WINDOW_MANAGER; exit 1; }
if ! process_running /system/bin/demolauncher; then
    on -d -u 1000:1000,36 /bin/sh "$0" --launcher-user </dev/null >>"$logdir/launcher.log" 2>&1
fi
wait_process /system/bin/demolauncher || { echo PI400_DESKTOP_ERROR_LAUNCHER; exit 1; }
echo PI400_DESKTOP_PROCESSES_READY
