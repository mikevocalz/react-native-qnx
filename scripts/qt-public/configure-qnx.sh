#!/usr/bin/env bash
# QNX aarch64le target from the public 6.12.0 source. Options and target list
# match the evaluation's configure-qt-qnx.sh; differences: public source path,
# host tools built from the same public source, -no-pch, separate prefixes.
set -euo pipefail
source "$(dirname "$0")/layout.sh"
source "$QNX_SDP/qnxsdp-env.sh" >/dev/null
export LANG=C.UTF-8
host=$QT_PUBLIC_BUILD/host-build/qtbase
export LD_LIBRARY_PATH=$host/lib
src=$QT_PUBLIC_SRC
build=$QT_PUBLIC_BUILD/qnx-build
mkdir -p "$build"; cd "$build"
"$src/configure" -developer-build -release -optimize-size -no-pch -nomake tests -nomake examples \
  -prefix /data/qt612-public -extprefix "$QT_PUBLIC_BUILD/qnx-stage" \
  -opengl es2 -qt-host-path "$host" -submodules qtbase,qtshadertools,qtdeclarative -- \
  -DCMAKE_TOOLCHAIN_FILE="$QT_PUBLIC_SCRIPTS/qt612-public-qnx-aarch64le.cmake" \
  -DCMAKE_SYSTEM_VERSION=800 -DFEATURE_no_prefix=OFF \
  -DFEATURE_widgets=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF \
  -DFEATURE_dbus=OFF -DFEATURE_icu=OFF -DFEATURE_fontconfig=OFF \
  -DFEATURE_system_freetype=OFF -DFEATURE_system_harfbuzz=OFF \
  -DFEATURE_system_zlib=OFF -DFEATURE_system_png=OFF -DFEATURE_system_jpeg=OFF \
  -DFEATURE_system_pcre2=OFF -DFEATURE_vulkan=OFF -DFEATURE_qml_debug=OFF \
  -DFEATURE_qqnx_imf=OFF
cmake --build . --parallel "${JOBS:-3}" --target \
  Core Network Gui Qml Quick QuickShapes QQnxIntegrationPlugin \
  qmlplugin modelsplugin workerscriptplugin qtquick2plugin qmlshapesplugin
