D=/data/home/qnxuser/rnqnx-fmt-1; Q=/data/home/qnxuser/qt612-public-o2
chmod +x $D/fmtprobe
export LD_LIBRARY_PATH="$D:$Q/lib:/data/pi400-graphics-compat/lib/dll:/data/pi400-graphics-compat/usr/lib:/data/pi400-graphics-compat/usr/lib/graphics/rpi4-drm:/usr/lib"
export QT_QPA_PLATFORM=qnx:no-fullscreen QT_QPA_PLATFORM_PLUGIN_PATH=$Q/plugins/platforms QT_QPA_FONTDIR=/usr/share/fonts/dejavu
cd $D
echo "### A: tile-1 app plugins (copy) + runtime, as release launcher: QT_PLUGIN_PATH=app/plugins:QT_ROOT/plugins"
QT_PLUGIN_PATH=$D/base/plugins:$Q/plugins QT_DEBUG_PLUGINS=1 ./fmtprobe anim.gif anim.webp still.png still.jpg still.webp still.tiff 2>&1 | grep -E "imageformats|supported|format=|QMovie|Cannot|cannot|Error|error" 
echo "### B: with new plugins"
QT_PLUGIN_PATH=$D/plugins:$D/base/plugins:$Q/plugins ./fmtprobe anim.gif anim.webp still.png still.jpg still.webp still.tiff 2>&1
