# Shared paths for the public-source Qt build. Sourced by the other scripts.
# Defaults match the qnx805-build container used on 2026-11-12, where the
# host's ~/qnx-pi400-8.0.5 is mounted at /work. Override any of them.
#
#   $QT_PUBLIC_ROOT/download/qt-everywhere-src-6.12.0.tar.xz   archive
#   $QT_PUBLIC_ROOT/download/cve-patches/*.diff                 qtbase diffs
#   $QT_PUBLIC_ROOT/src/qt-everywhere-src-6.12.0               extracted tree
#   $QT_PUBLIC_ROOT/runtime                                     staged runtime
#   $QT_PUBLIC_BUILD/host-build, qnx-build, qnx-stage           build trees
#   $QNX_SDP/qnxsdp-env.sh                                      QNX SDP 8.0.5
QT_PUBLIC_ROOT=${QT_PUBLIC_ROOT:-/work/qt-6.12.0-public}
QT_PUBLIC_BUILD=${QT_PUBLIC_BUILD:-/home/qnxbuild/qt-6.12.0-public}
QNX_SDP=${QNX_SDP:-/work/qnx800}
QT_PUBLIC_SRC=$QT_PUBLIC_ROOT/src/qt-everywhere-src-6.12.0
QT_PUBLIC_SCRIPTS=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
export QT_PUBLIC_ROOT QT_PUBLIC_BUILD QNX_SDP QT_PUBLIC_SRC QT_PUBLIC_SCRIPTS
