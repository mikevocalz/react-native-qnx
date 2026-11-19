#!/usr/bin/env bash
# QtSvg for QNX aarch64le from the same public 6.12.0 archive, built against the
# existing QNX qtbase (configure-qnx.sh) with qt-configure-module, the way
# build-imageformats-qnx.sh builds qtimageformats. qtbase has no SVG support;
# the qtsvg module adds libQt6Svg and the `qsvg` image-format plugin, which
# lets QImageReader (and so an <Image> in react-native-svg) decode an SVG
# document. Qt Quick finds the plugin through QT_PLUGIN_PATH. Runs inside
# qnx805-build after configure-qnx.sh.
# Output: $SVG_BUILD/lib/libQt6Svg.so.6.12.0 and
#         $SVG_BUILD/plugins/imageformats/libqsvg.so
# Environment: SVG_BUILD (default $QT_PUBLIC_BUILD/svg-qnx), SVG_SRC (default
# $QT_PUBLIC_ROOT/src), JOBS (default 2).
set -euo pipefail
source "$(dirname "$0")/layout.sh"
source "$QNX_SDP/qnxsdp-env.sh" >/dev/null
export LANG=C.UTF-8
export LD_LIBRARY_PATH=$QT_PUBLIC_BUILD/host-build/qtbase/lib
archive=$QT_PUBLIC_ROOT/download/qt-everywhere-src-6.12.0.tar.xz
echo "98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165  $archive" | sha256sum -c -
srcparent=${SVG_SRC:-$QT_PUBLIC_ROOT/src}
build=${SVG_BUILD:-$QT_PUBLIC_BUILD/svg-qnx}
top=qt-everywhere-src-6.12.0
if [ ! -d "$srcparent/$top/qtsvg" ]; then
  mkdir -p "$srcparent"
  tar -xJf "$archive" -C "$srcparent" --exclude='*/tests' $top/qtsvg
fi
mkdir -p "$build"; cd "$build"
"$QT_PUBLIC_BUILD/qnx-build/qtbase/bin/qt-configure-module" "$srcparent/$top/qtsvg"
flags=$(sed -n 's/^CMAKE_CXX_FLAGS_RELEASE:STRING=//p' CMakeCache.txt)
case " $flags " in
  *" -O2 "*) ;;
  *) echo "CMAKE_CXX_FLAGS_RELEASE is '$flags'; ADR-0017 requires -O2" >&2; exit 1 ;;
esac
cmake --build . --parallel "${JOBS:-2}" --target Svg QSvgPlugin
ls -l lib plugins/imageformats
