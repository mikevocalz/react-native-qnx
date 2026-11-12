# Sourced by the public-source Qt proof launchers. Only the public-source Qt
# runtime is on the library and plugin paths; /data/qt612 (evaluation) is not.
Q=/data/home/qnxuser/qt612-public
export LD_LIBRARY_PATH="/data/home/qnxuser/rnqnx-hello-world:$Q/lib:/data/pi400-graphics-compat/lib/dll:/data/pi400-graphics-compat/usr/lib:/data/pi400-graphics-compat/usr/lib/graphics/rpi4-drm:/usr/lib"
export QT_QPA_PLATFORM=qnx:no-fullscreen
export QT_QPA_PLATFORM_PLUGIN_PATH=$Q/plugins/platforms
export QT_PLUGIN_PATH=$Q/plugins
export QML_IMPORT_PATH=$Q/qml
export QML2_IMPORT_PATH=$Q/qml
export QT_QPA_FONTDIR=/usr/share/fonts/dejavu
export QSG_RHI_BACKEND=opengl
export QT_FORCE_STDERR_LOGGING=1
export DL_DEBUG=libs
