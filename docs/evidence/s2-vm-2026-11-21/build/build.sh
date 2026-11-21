#!/bin/bash
# S2 VM: JS-thread test for QNX aarch64le. build.sh [configure|build|rebuild]
set -euo pipefail
R=/home/qnxbuild/s2vm
S=$R/src
B=$R/qnx-build
STEP=${1:-all}
source /work/qnx800/qnxsdp-env.sh >/dev/null
export PATH=/home/qnxbuild/tools/cmake-3.31.9-linux-x86_64/bin:$PATH
cd $S
HC=$S/$(ls -d node_modules/.pnpm/hermes-compiler@260318099.0.5/node_modules/hermes-compiler/hermesc/linux64-bin)/hermesc
P=/work/pr102-consumer/build/qnx/native/_deps
EXTRA=${EXTRA:-}
if [ "$STEP" = all ] || [ "$STEP" = configure ]; then
  cmake --preset qnx-aarch64-release -S $S -B $B \
    -DRNQNX_BUILD_REACT_QNX=ON -DRNQNX_BUILD_HERMES_QNX=ON -DRNQNX_FETCH_REACT_NATIVE_DEPS=ON \
    -DRNQNX_ENABLE_WINDOW_CHROME=ON -DRNQNX_ENABLE_INPUT_LIFECYCLE=ON -DRNQNX_ENABLE_APP_STATE=ON \
    -DRNQNX_ENABLE_WORKLETS=ON -DRNQNX_ENABLE_REANIMATED=ON -DRNQNX_ENABLE_GESTURE_HANDLER=ON \
    -DRNQNX_ENABLE_WEBSOCKETS=ON -DQt6WebSockets_DIR=/home/qnxbuild/qtws-qnx/lib/cmake/Qt6WebSockets \
    -DRNQNX_BUILD_JS_THREAD_QNX_TEST=ON -DRNQNX_JS_THREAD_TEST_HERMESC=$HC \
    -DRNQNX_HERMES_QNX_DIR=/home/qnxbuild/hermes-qnx-full \
    -DQt6_DIR=/home/qnxbuild/qt-6.12.0-public/qnx-build/qtbase/lib/cmake/Qt6 \
    -DQT_HOST_PATH=/home/qnxbuild/qt-6.12.0-public/host-build/qtbase \
    -DRNQNX_HERMES_QNX_SOURCE_DIR=/work/rn-source/third-party/hermes-260318099.0.5 -DRNQNX_INPUT_TEST_HERMESC=$HC \
    -DRNQNX_ENABLE_HOST_SANITIZERS=OFF -DRNQNX_WARNINGS_AS_ERRORS=OFF \
    "-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG -Wno-error=maybe-uninitialized -Wno-error=array-bounds -Wno-error=stringop-overflow -Wno-error=restrict -Wno-error=free-nonheap-object -Wno-error=dangling-pointer -Wno-error=use-after-free -Wno-error=null-dereference -Wno-error=stringop-overread -Wno-error=uninitialized" \
    "-DCMAKE_C_FLAGS_RELEASE=-O2 -DNDEBUG" -DCMAKE_TOOLCHAIN_FILE=$R/qnx-qt-public.toolchain.cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DFETCHCONTENT_SOURCE_DIR_RNQNX_BOOST=$P/rnqnx_boost-src \
    -DFETCHCONTENT_SOURCE_DIR_RNQNX_BOOST_CONFIG=$P/rnqnx_boost_config-src \
    -DFETCHCONTENT_SOURCE_DIR_RNQNX_DOUBLE_CONVERSION=$P/rnqnx_double_conversion-src \
    -DFETCHCONTENT_SOURCE_DIR_RNQNX_FAST_FLOAT=$P/rnqnx_fast_float-src \
    -DFETCHCONTENT_SOURCE_DIR_RNQNX_FMT=$P/rnqnx_fmt-src \
    -DFETCHCONTENT_SOURCE_DIR_RNQNX_FOLLY=$P/rnqnx_folly-src \
    -DFETCHCONTENT_SOURCE_DIR_RNQNX_GLOG=$P/rnqnx_glog-src $EXTRA
fi
if [ "$STEP" = all ] || [ "$STEP" = build ]; then
  cmake --build $B --target qt_react_host_js_thread_qnx_test --parallel 4 -- -k 0
  ls -la $B/bin
fi
