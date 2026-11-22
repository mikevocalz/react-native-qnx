#!/usr/bin/env bash
# SPDX audits for the base, qtimageformats and qtsvg build trees, then stage the
# runtime from them (README.md "Order"). Runs in the build container after
# single-pass-build.sh. Output: $QT_PUBLIC_ROOT/{audit-*.json,runtime,logs/}.
set -euo pipefail
source "$(dirname "$0")/layout.sh"
source "$QNX_SDP/qnxsdp-env.sh" >/dev/null 2>&1
here=$QT_PUBLIC_SCRIPTS
logs=${STAGE_LOG_DIR:-$QT_PUBLIC_ROOT/logs}
mkdir -p "$logs"
top=qt-everywhere-src-6.12.0
svg_src=${SVG_SRC:-$QT_PUBLIC_ROOT/src-svg}
python3 -I "$here/spdx-audit.py" "$QT_PUBLIC_BUILD/qnx-build" "$QT_PUBLIC_SRC" \
  "$QT_PUBLIC_ROOT/audit-base.json" Core Network Gui OpenGL Qml QmlMeta QmlModels \
  QmlWorkerScript Quick QuickShapes QQnxIntegrationPlugin qmlplugin modelsplugin \
  workerscriptplugin qtquick2plugin qmlshapesplugin QTlsBackendOpenSSLPlugin QJpegPlugin QGifPlugin \
  > "$logs/audit-base.log" 2>&1
python3 -I "$here/spdx-audit.py" "${IMAGEFORMATS_BUILD:-$QT_PUBLIC_BUILD/imageformats-qnx}" \
  "${IMAGEFORMATS_SRC:-$QT_PUBLIC_ROOT/src}/$top" "$QT_PUBLIC_ROOT/audit-imageformats.json" \
  QWebpPlugin QTiffPlugin QTgaPlugin QICNSPlugin QWbmpPlugin > "$logs/audit-imageformats.log" 2>&1
python3 -I "$here/spdx-audit.py" "${SVG_BUILD:-$QT_PUBLIC_BUILD/svg-qnx}" "$svg_src/$top" \
  "$QT_PUBLIC_ROOT/audit-svg.json" Svg QSvgPlugin > "$logs/audit-svg.log" 2>&1
# QT_EVAL_MANIFEST points nowhere, so no evaluation file is read.
QT_EVAL_MANIFEST=/nonexistent python3 -I "$here/stage-runtime.py" "$QT_PUBLIC_ROOT/runtime" \
  "$QT_PUBLIC_ROOT/audit-base.json" "$QT_PUBLIC_ROOT/audit-imageformats.json" \
  "$QT_PUBLIC_ROOT/audit-svg.json" > "$logs/stage-runtime.log" 2>&1
tail -20 "$logs/stage-runtime.log"
