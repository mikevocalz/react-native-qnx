#!/usr/bin/env bash
# Host tools from the public 6.12.0 source. Same options as the evaluation's
# configure-qt-host.sh, plus -no-pch to bound disk use on this host.
set -euo pipefail
source "$(dirname "$0")/layout.sh"
src=$QT_PUBLIC_SRC
build=$QT_PUBLIC_BUILD/host-build
mkdir -p "$build"; cd "$build"
"$src/configure" -developer-build -release -no-opengl -no-pch -nomake tests -nomake examples \
  -submodules qtbase,qtshadertools,qtdeclarative -- \
  -DFEATURE_opengl=OFF -DFEATURE_vulkan=OFF -DFEATURE_xcb=OFF \
  -DFEATURE_dbus=OFF -DFEATURE_widgets=OFF -DFEATURE_printsupport=OFF \
  -DFEATURE_sql=OFF -DFEATURE_icu=OFF -DFEATURE_fontconfig=OFF \
  -DFEATURE_system_freetype=OFF -DFEATURE_system_harfbuzz=OFF
cmake --build . --target host_tools --parallel "${JOBS:-2}"
