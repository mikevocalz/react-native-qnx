#!/usr/bin/env bash
# Qt WebSockets for QNX aarch64le from the same public 6.12.0 archive, built
# against the existing QNX qtbase (configure-qnx.sh) with qt-configure-module.
# The development client (RNQNX_ENABLE_DEV_SUPPORT) needs it for Metro's /hot
# and /message sockets. Runs inside qnx805-build after configure-qnx.sh.
# Output: $QT_PUBLIC_BUILD/qtwebsockets-qnx/lib/libQt6WebSockets.so.6.12.0 and
# lib/cmake/Qt6WebSockets for -DQt6WebSockets_DIR.
set -euo pipefail
source "$(dirname "$0")/layout.sh"
source "$QNX_SDP/qnxsdp-env.sh" >/dev/null
export LANG=C.UTF-8
archive=$QT_PUBLIC_ROOT/download/qt-everywhere-src-6.12.0.tar.xz
echo "98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165  $archive" | sha256sum -c -
src=$QT_PUBLIC_BUILD/qtwebsockets-src
build=$QT_PUBLIC_BUILD/qtwebsockets-qnx
if [ ! -d "$src/qt-everywhere-src-6.12.0/qtwebsockets" ]; then
  mkdir -p "$src"
  tar -xJf "$archive" -C "$src" \
    --exclude='*/tests' --exclude='*/examples' qt-everywhere-src-6.12.0/qtwebsockets
fi
mkdir -p "$build"; cd "$build"
"$QT_PUBLIC_BUILD/qnx-build/qtbase/bin/qt-configure-module" \
  "$src/qt-everywhere-src-6.12.0/qtwebsockets"
flags=$(sed -n 's/^CMAKE_CXX_FLAGS_RELEASE:STRING=//p' CMakeCache.txt)
case " $flags " in
  *" -O2 "*) ;;
  *) echo "CMAKE_CXX_FLAGS_RELEASE is '$flags'; ADR-0017 requires -O2" >&2; exit 1 ;;
esac
cmake --build . --parallel "${JOBS:-3}" --target WebSockets
ls -l lib/libQt6WebSockets.so.6.12.0
