# shellcheck shell=bash disable=SC2034
# Shared paths for the Qt Multimedia add-on build. Sourced by the other scripts.
MEDIA_ROOT=${MEDIA_ROOT:-/home/qnxbuild/media}
QT_PUBLIC_BUILD=${QT_PUBLIC_BUILD:-/home/qnxbuild/qt-6.12.0-public}
QT_PUBLIC_SCRIPTS=${QT_PUBLIC_SCRIPTS:-/home/qnxbuild/qt-public-scripts-main/qt-public}
QNX_SDP=${QNX_SDP:-/work/qnx800}
# Where the add-on lives on the board. Never the Qt core directory.
MEDIA_PREFIX=${MEDIA_PREFIX:-/data/home/qnxuser/qt612-media}
FFMPEG_VERSION=8.0
FFMPEG_SHA256=b2751fccb6cc4c77708113cd78b561059b6fa904b24162fa0be2d60273d27b8e
QTMM_VERSION=6.12.0
QTMM_SHA256=3143f53b64257ba2a0685c940f5dce476c75871ff86b516de45cfd54ea93a62d
export MEDIA_ROOT QT_PUBLIC_BUILD QT_PUBLIC_SCRIPTS QNX_SDP MEDIA_PREFIX
