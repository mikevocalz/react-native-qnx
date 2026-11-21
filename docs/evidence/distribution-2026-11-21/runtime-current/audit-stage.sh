set -euo pipefail
source /work/qnx800/qnxsdp-env.sh >/dev/null 2>&1
S=/work/dist-root2/scripts
B=/home/qnxbuild/qt-6.12.0-public
SRC=/work/qt-6.12.0-public/src/qt-everywhere-src-6.12.0
IFS_SRC=/work/dist-root2/src/qt-everywhere-src-6.12.0
IFB=/home/qnxbuild/dist-build2/imageformats-qnx
OUT=/work/dist-root2
python3 -I $S/spdx-audit.py $B/qnx-build $SRC $OUT/audit-base.json Core Network Gui OpenGL Qml QmlMeta QmlModels QmlWorkerScript Quick QuickShapes QQnxIntegrationPlugin qmlplugin modelsplugin workerscriptplugin qtquick2plugin qmlshapesplugin QTlsBackendOpenSSLPlugin QJpegPlugin QGifPlugin > $OUT/logs/audit-base.log 2>&1; echo base rc=$?
python3 -I $S/spdx-audit.py $IFB $IFS_SRC $OUT/audit-imageformats.json QWebpPlugin QTiffPlugin QTgaPlugin QICNSPlugin QWbmpPlugin > $OUT/logs/audit-imageformats.log 2>&1; echo imageformats rc=$?
python3 -I $S/spdx-audit.py $B/svg-qnx $SRC $OUT/audit-svg.json Svg QSvgPlugin > $OUT/logs/audit-svg.log 2>&1; echo svg rc=$?
