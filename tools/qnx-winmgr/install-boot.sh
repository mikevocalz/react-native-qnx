#!/bin/sh
# Makes rnqnx-winmgr the boot window manager on the Pi 400. Run as root on the
# Pi, from the directory holding rnqnx-winmgr and rnqnx-desktop.sh:
#   sh install-boot.sh            install (backs up full_startup.sh first)
#   sh install-boot.sh --undo     put the backed-up full_startup.sh back
# Only the window manager changes: the same launcher, Screen, mouse and
# keyboard start as before. Nothing restarts now; the change takes effect on
# the next boot, or at once by hand with the commands it prints.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
# The three paths can be moved for a dry run on copies.
startup=${RNQNX_STARTUP:-/system/etc/startup/full_startup.sh}
backup=$startup.before-rnqnx-winmgr
desktop=${RNQNX_DESKTOP:-/system/etc/startup/rnqnx-desktop.sh}
dest=${RNQNX_WINMGR_DIR:-/data/rnqnx-winmgr}
stock_exec='exec /bin/sh /usr/etc/startup/full_startup.sh.before-rnqnx-autostart'

[ "$(id -u)" = 0 ] || { echo 'install-boot.sh: run as root'; exit 1; }

if [ "${1:-}" = --undo ]; then
    [ -f "$backup" ] || { echo "install-boot.sh: no backup at $backup"; exit 1; }
    cp -p "$backup" "$startup"
    echo "restored $startup from $backup ($desktop and $dest stay; unused)"
    exit 0
fi

[ -x "$here/rnqnx-winmgr" ] || { echo "install-boot.sh: missing $here/rnqnx-winmgr"; exit 1; }
[ -f "$here/rnqnx-desktop.sh" ] || { echo "install-boot.sh: missing $here/rnqnx-desktop.sh"; exit 1; }
grep -qx "$stock_exec" "$startup" || grep -qx "exec /bin/sh $desktop" "$startup" || {
    echo "install-boot.sh: $startup does not end with the expected line:"
    echo "  $stock_exec"
    echo 'Not changing it.'
    exit 1
}

# The binary runs as root at boot, so it lives in a root-owned directory,
# not in a user's home.
mkdir -p "$dest"
chown root:root "$dest"
chmod 0755 "$dest"
cp "$here/rnqnx-winmgr" "$dest/rnqnx-winmgr.new"
chown root:root "$dest/rnqnx-winmgr.new"
chmod 0755 "$dest/rnqnx-winmgr.new"
mv -f "$dest/rnqnx-winmgr.new" "$dest/rnqnx-winmgr"

cp "$here/rnqnx-desktop.sh" "$desktop"
chown root:root "$desktop"
chmod 0555 "$desktop"

[ -f "$backup" ] || cp -p "$startup" "$backup"
tmp=$startup.rnqnx-tmp
sed "s|^$stock_exec\$|exec /bin/sh $desktop|" "$startup" > "$tmp"
chmod 0555 "$tmp"
mv -f "$tmp" "$startup"

echo "installed $dest/rnqnx-winmgr and $desktop"
echo "backup: $backup (undo: sh $0 --undo)"
echo 'Takes effect on the next boot. To switch now without rebooting:'
echo "  slay fullscreen-winmgr demolauncher; on -d $dest/rnqnx-winmgr -s demolauncher,st,gtk; sleep 2; on -d demolauncher -a /system/etc/desktop_files"
