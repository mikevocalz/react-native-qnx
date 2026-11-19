#!/usr/bin/env bash
# Qt image format plugins for QNX aarch64le from the same public 6.12.0
# archive, built against the existing QNX qtbase (configure-qnx.sh) with
# qt-configure-module. qtbase ships only the GIF, JPEG and ICO plugins; the
# qtimageformats module adds WebP (animated and still), TIFF, TGA, ICNS and
# WBMP. Qt Quick's Image and QMovie find them through QT_PLUGIN_PATH, which is
# how ExpoImageItem plays an animated WebP. Runs inside qnx805-build after
# configure-qnx.sh.
# Output: $IMAGEFORMATS_BUILD/plugins/imageformats/lib{qwebp,qtiff,qtga,qicns,qwbmp}.so
# Environment: IMAGEFORMATS_BUILD (default $QT_PUBLIC_BUILD/imageformats-qnx),
# IMAGEFORMATS_SRC (default $QT_PUBLIC_ROOT/src), JOBS (default 2).
set -euo pipefail
source "$(dirname "$0")/layout.sh"
source "$QNX_SDP/qnxsdp-env.sh" >/dev/null
export LANG=C.UTF-8
export LD_LIBRARY_PATH=$QT_PUBLIC_BUILD/host-build/qtbase/lib
archive=$QT_PUBLIC_ROOT/download/qt-everywhere-src-6.12.0.tar.xz
echo "98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165  $archive" | sha256sum -c -
srcparent=${IMAGEFORMATS_SRC:-$QT_PUBLIC_ROOT/src}
build=${IMAGEFORMATS_BUILD:-$QT_PUBLIC_BUILD/imageformats-qnx}
top=qt-everywhere-src-6.12.0
if [ ! -d "$srcparent/$top/qtimageformats" ]; then
  mkdir -p "$srcparent"
  tar -xJf "$archive" -C "$srcparent" --exclude='*/tests' $top/qtimageformats
fi
mkdir -p "$build"; cd "$build"
# Bundled libwebp and libtiff: the board has neither, and the plugins link
# them statically.
"$QT_PUBLIC_BUILD/qnx-build/qtbase/bin/qt-configure-module" \
  "$srcparent/$top/qtimageformats" -- \
  -DFEATURE_system_webp=OFF -DFEATURE_system_tiff=OFF \
  -DFEATURE_jasper=OFF -DFEATURE_mng=OFF
flags=$(sed -n 's/^CMAKE_CXX_FLAGS_RELEASE:STRING=//p' CMakeCache.txt)
case " $flags " in
  *" -O2 "*) ;;
  *) echo "CMAKE_CXX_FLAGS_RELEASE is '$flags'; ADR-0017 requires -O2" >&2; exit 1 ;;
esac
cmake --build . --parallel "${JOBS:-2}" --target \
  QWebpPlugin QTiffPlugin QTgaPlugin QICNSPlugin QWbmpPlugin
ls -l plugins/imageformats/
