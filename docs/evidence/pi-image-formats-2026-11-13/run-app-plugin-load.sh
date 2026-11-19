D=/data/home/qnxuser/rnqnx-fmt-1; Q=/data/home/qnxuser/qt612-public-o2
rm -rf $D/appcopy; mkdir -p $D/appcopy; cp -R /data/home/qnxuser/rnqnx-tile-1/. $D/appcopy/
A=$D/appcopy
export LD_LIBRARY_PATH="$A:$Q/lib:/data/pi400-graphics-compat/lib/dll:/data/pi400-graphics-compat/usr/lib:/data/pi400-graphics-compat/usr/lib/graphics/rpi4-drm:/usr/lib"
export QT_QPA_PLATFORM=qnx:no-fullscreen QT_QPA_PLATFORM_PLUGIN_PATH=$Q/plugins/platforms QML_IMPORT_PATH=$Q/qml QML2_IMPORT_PATH=$Q/qml QT_QPA_FONTDIR=/usr/share/fonts/dejavu QT_FORCE_STDERR_LOGGING=1
unset QT_QUICK_BACKEND
cd $A
for variant in A B; do
  if [ $variant = A ]; then export QT_PLUGIN_PATH=$A/plugins:$Q/plugins; else mkdir -p $A/plugins/imageformats; cp $D/plugins/imageformats/*.so $A/plugins/imageformats/; export QT_PLUGIN_PATH=$A/plugins:$Q/plugins; fi
  echo "### variant $variant: ls plugins/imageformats: $(ls $A/plugins/imageformats | tr '\n' ' ')"
  QT_DEBUG_PLUGINS=1 timeout 60 ./org.rnqnx.showcase --window-size 800x600 --screenshot $D/shot-$variant.png > $D/app-$variant.log 2>&1; echo "exit $?"
  grep -E "imageformats" $D/app-$variant.log | grep -E "loaded|Got keys|Found metadata|Cannot load|not a plugin" | sed 's/^/  /' | head -20
  grep -c "" $D/app-$variant.log
done
