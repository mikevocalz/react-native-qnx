#!/bin/sh
# Launch a media app on the Pi 400 by hand; not part of boot or autostart.
# Same environment as run-chrome-app.sh (docs/getting-started/pi400-desktop-app.md)
# plus the Qt Multimedia add-on, which lives in its own directory so the Qt core
# runtime in qt612-public-o2 is never modified.
#   run-media-app.sh [app-dir] [app args...]
A=${1:-/data/home/qnxuser/rnqnx-media-app}
[ $# -gt 0 ] && shift
Q=/data/home/qnxuser/qt612-public-o2
M=/data/home/qnxuser/qt612-media
export LD_LIBRARY_PATH="$A:$M/lib:$Q/lib:/data/pi400-graphics-compat/lib/dll:/data/pi400-graphics-compat/usr/lib:/data/pi400-graphics-compat/usr/lib/graphics/rpi4-drm:/usr/lib"
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-qnx:no-fullscreen}
export QT_QPA_PLATFORM_PLUGIN_PATH=/data/home/qnxuser/glfix/plugins/platforms
# Add-on plugins (multimedia/ffmpeg) first, then the Qt core plugins.
export QT_PLUGIN_PATH="$M/plugins:/data/home/qnxuser/glfix/plugins"
export QML_IMPORT_PATH="$M/qml:$Q/qml"
export QML2_IMPORT_PATH="$M/qml:$Q/qml"
export QT_QPA_FONTDIR=/usr/share/fonts/dejavu
export QT_FORCE_STDERR_LOGGING=1
export QT_MEDIA_BACKEND=ffmpeg
[ -t 1 ] || exec >>/tmp/rnqnx-media.log 2>&1
cd "$A" || exit 1
exec "${RNQNX_MEDIA_EXE:-./org.rnqnx.showcase}" "$@"
