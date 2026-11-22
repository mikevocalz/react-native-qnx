#!/usr/bin/env bash
# Link the repository's playground app (rnqnx_playground_qnx) against the
# public-source Qt build that single-pass-build.sh produced. Fresh configure and
# build directory every time. Runs in the same container as the Qt build.
#
# Needs: the repository checkout at $APP_SRC with node_modules installed
# (pnpm install --frozen-lockfile), a QNX Hermes build tree at $HERMES_QNX_DIR
# and its source at $HERMES_QNX_SRC (scripts/prepare-hermes-qnx.sh,
# scripts/configure-hermes-qnx.sh), and the React Native third-party sources at
# $RN_DEPS_DIR (one rnqnx_*-src directory each: boost, boost_config,
# double_conversion, fast_float, fmt, folly, glog), because
# FETCHCONTENT_FULLY_DISCONNECTED is on.
#
# RPATH: the application is linked with CMAKE_BUILD_WITH_INSTALL_RPATH and
# --enable-new-dtags, so it carries DT_RUNPATH $APP_RUNPATH and no build-host
# directory.
#
# Environment: APP_SRC (repo root), APP_BUILD (default $QT_PUBLIC_BUILD/app-qnx),
# HERMES_QNX_DIR, HERMES_QNX_SRC, RN_DEPS_DIR, APP_RUNPATH (default
# /data/qt612-public/lib), JOBS (default 4).
set -euo pipefail
source "$(dirname "$0")/layout.sh"
# The licensed SDK is supplied by the build host, not this source bundle.
# shellcheck source=/dev/null
source "$QNX_SDP/qnxsdp-env.sh" >/dev/null
export LANG=C.UTF-8
: "${APP_SRC:?}" "${HERMES_QNX_DIR:?}" "${HERMES_QNX_SRC:?}" "${RN_DEPS_DIR:?}"
APP_SRC=$(cd "$APP_SRC" && pwd -P)
app_toolchain="$APP_SRC/vnext/cmake/qnx-aarch64le.toolchain.cmake"
test -f "$app_toolchain" || { echo "missing application toolchain: $app_toolchain" >&2; exit 2; }
build=${APP_BUILD:-$QT_PUBLIC_BUILD/app-qnx}
runpath=${APP_RUNPATH:-/data/qt612-public/lib}
test ! -e "$build/CMakeCache.txt" || { echo "refusing to reuse $build" >&2; exit 2; }
# React Native builds its own sources with -Werror. At -O2 GCC 12 (QCC 12.2.0)
# reports false maybe-uninitialized and similar diagnostics in them (first seen
# 2026-11-22 in ResizeObserverManager.cpp). Keep -Werror for everything else and
# demote these, the list the 2026-11-19 PR 149 -O2 build used.
cxx_release="-O2 -DNDEBUG -Wno-error=maybe-uninitialized -Wno-error=array-bounds"
cxx_release+=" -Wno-error=stringop-overflow -Wno-error=restrict -Wno-error=free-nonheap-object"
cxx_release+=" -Wno-error=dangling-pointer -Wno-error=use-after-free -Wno-error=null-dereference"
cxx_release+=" -Wno-error=stringop-overread -Wno-error=uninitialized"
deps=()
for d in boost boost_config double_conversion fast_float fmt folly glog; do
  test -d "$RN_DEPS_DIR/rnqnx_$d-src"
  deps+=("-DFETCHCONTENT_SOURCE_DIR_RNQNX_${d^^}=$RN_DEPS_DIR/rnqnx_$d-src")
done
cmake -S "$APP_SRC" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$QT_PUBLIC_SCRIPTS/qnx-app-qt-public.toolchain.cmake" \
  -DRNQNX_APP_TOOLCHAIN_PATH:FILEPATH="$app_toolchain" \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_CXX_FLAGS_RELEASE=$cxx_release" "-DCMAKE_C_FLAGS_RELEASE=-O2 -DNDEBUG" \
  -DBUILD_TESTING=OFF -DRNQNX_ENABLE_HOST_SANITIZERS=OFF -DRNQNX_WARNINGS_AS_ERRORS=OFF \
  -DRNQNX_BUILD_REACT_QNX=ON -DRNQNX_BUILD_HERMES_QNX=ON \
  -DRNQNX_FETCH_REACT_NATIVE_DEPS=ON -DFETCHCONTENT_FULLY_DISCONNECTED=ON "${deps[@]}" \
  -DRNQNX_BUILD_PLAYGROUND_QNX=ON \
  -DRNQNX_HERMES_QNX_DIR="$HERMES_QNX_DIR" -DRNQNX_HERMES_QNX_SOURCE_DIR="$HERMES_QNX_SRC" \
  -DQt6_DIR="$QT_PUBLIC_BUILD/qnx-build/qtbase/lib/cmake/Qt6" \
  -DQT_HOST_PATH="$QT_PUBLIC_BUILD/host-build/qtbase" \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON "-DCMAKE_INSTALL_RPATH=$runpath"
cmake --build "$build" --target rnqnx_playground_qnx --parallel "${JOBS:-4}"
ls -l "$build/bin/rnqnx_playground_qnx"
