#!/bin/sh
# Starts io-snd at boot on the Pi 400, so /dev/snd exists before any app opens
# an audio device. Run as root on the board:
#   sh install-audio-boot.sh            install (backs up full_startup.sh first)
#   sh install-audio-boot.sh --undo     put the backed-up file back
# io-snd reads /etc/system/config/sound/io_snd.conf, which on this image
# configures the 3.5 mm headphone jack (bcm2711_pwm). The block goes in front of
# the line that hands over to the desktop script, and runs only when /dev/snd
# is missing, so a second io-snd is never started. Nothing restarts now.
set -eu
startup=${RNQNX_STARTUP:-/system/etc/startup/full_startup.sh}
backup=$startup.before-audio
marker='# rnqnx-audio: io-snd'
handoff='exec /bin/sh /system/etc/startup/rnqnx-desktop.sh'

[ "$(id -u)" = 0 ] || [ -n "${RNQNX_DRY_RUN:-}" ] || { echo 'install-audio-boot.sh: run as root'; exit 1; }

if [ "${1:-}" = --undo ]; then
    [ -f "$backup" ] || { echo "install-audio-boot.sh: no backup at $backup"; exit 1; }
    cp -p "$backup" "$startup.audio-tmp"
    mv -f "$startup.audio-tmp" "$startup"
    echo "restored $startup from $backup"
    exit 0
fi

grep -qx "$handoff" "$startup" || {
    echo "install-audio-boot.sh: $startup does not contain the line:"
    echo "  $handoff"
    echo 'Not changing it.'
    exit 1
}
if grep -qxF "$marker" "$startup"; then
    echo "already installed in $startup"
    exit 0
fi

cp -p "$startup" "$backup.audio-tmp"
mv -f "$backup.audio-tmp" "$backup"
tmp=$startup.audio-tmp
awk -v hand="$handoff" -v mark="$marker" '
    $0 == hand {
        print mark
        print "if [ ! -e /dev/snd ] && [ -f /etc/system/config/sound/io_snd.conf ]; then"
        print "    io-snd -c /etc/system/config/sound/io_snd.conf </dev/null >>/tmp/io-snd.log 2>&1"
        print "fi"
    }
    { print }
' "$startup" > "$tmp"
if ! sh -n "$tmp" || ! grep -qxF "$marker" "$tmp" || ! grep -qx "$handoff" "$tmp"; then
    rm -f "$tmp"
    echo 'install-audio-boot.sh: the patched file failed its check; nothing changed'
    exit 1
fi
chmod --reference="$startup" "$tmp" 2>/dev/null || chmod 0555 "$tmp"
mv -f "$tmp" "$startup"
echo "installed: $startup now starts io-snd (backup: $backup)"
echo 'It takes effect at the next boot. To start it now: io-snd -c /etc/system/config/sound/io_snd.conf'
