#!/usr/bin/env bash
# Qt Multimedia 6.12.0 (public source) against the existing Qt 6.12.0 QNX build
# tree and the FFmpeg from build-ffmpeg.sh. Only the Multimedia and
# MultimediaQuick libraries, the QML module and the ffmpeg plugin are built.
# The mmrenderer plugin is off: it shows video in a separate Screen window,
# which cannot compose with Qt Quick items (docs/getting-started/media.md).
# Audio goes through the Multimedia library's own io-snd backend (QNX 8).
set -euo pipefail
source "$(dirname "$0")/layout.sh"
source "$QNX_SDP/qnxsdp-env.sh" >/dev/null
export QT_PUBLIC_SRC=${QT_PUBLIC_SRC:-/work/qt-source/qt-everywhere-src-6.12.0}
cd "$MEDIA_ROOT"
if [ ! -f qtmm.tar.xz ]; then
  curl -sSL -o qtmm.tar.xz https://download.qt.io/official_releases/qt/6.12/$QTMM_VERSION/submodules/qtmultimedia-everywhere-src-$QTMM_VERSION.tar.xz
fi
echo "$QTMM_SHA256  qtmm.tar.xz" | sha256sum -c -
[ -d qtmultimedia-everywhere-src-$QTMM_VERSION ] || tar xf qtmm.tar.xz
FF="$MEDIA_ROOT/ff-stage$MEDIA_PREFIX"
B=$QT_PUBLIC_BUILD/qnx-build
host=$QT_PUBLIC_BUILD/host-build/qtbase
export LD_LIBRARY_PATH=$host/lib
mkdir -p qtmm-build; cd qtmm-build
# The Qt build tree and the FFmpeg stage live outside the SDP sysroot, so the
# toolchain's CMAKE_FIND_ROOT_PATH has to be extended after it is set.
cat > toolchain.cmake <<EOT
include($QT_PUBLIC_SCRIPTS/qt612-public-qnx-aarch64le.cmake)
list(APPEND CMAKE_FIND_ROOT_PATH $B/qtbase $B/qtdeclarative $B/qtshadertools $FF)
EOT
cmake -G Ninja -S ../qtmultimedia-everywhere-src-$QTMM_VERSION -B . \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$PWD/toolchain.cmake" \
  -DCMAKE_SYSTEM_VERSION=800 \
  -DQT_HOST_PATH="$host" -DQT_HOST_PATH_CMAKE_DIR="$host/lib/cmake" \
  -DCMAKE_PREFIX_PATH="$B/qtbase;$B/qtdeclarative;$B/qtshadertools;$QT_PUBLIC_BUILD/host-build/qtbase;$FF" \
  -DCMAKE_INSTALL_PREFIX="$MEDIA_PREFIX" \
  -DFFMPEG_DIR="$FF" -DFEATURE_ffmpeg=ON -DFEATURE_mmrenderer=OFF -DFEATURE_gstreamer=OFF \
  -DFEATURE_spatialaudio=OFF -DFEATURE_quick3d_spatialaudio=OFF \
  -DFEATURE_doc_snippets=OFF -DQT_FEATURE_doc_snippets=OFF -DQT_BUILD_DOC_SNIPPETS=OFF -DFEATURE_widgets=OFF -DBUILD_WITH_PCH=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF
cmake --build . --parallel "${JOBS:-6}"
DESTDIR="$MEDIA_ROOT/qtmm-stage" cmake --install .
