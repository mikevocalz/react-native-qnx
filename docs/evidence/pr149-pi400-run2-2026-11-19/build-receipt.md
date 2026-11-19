# PR 149 QNX aarch64 cross-build receipt, SVG on (2026-11-19)

Scope: cross-compile only. Not device acceptance. Nothing run on the Pi (192.168.2.2); nothing pushed, merged, undrafted or edited on GitHub.

## Source

- Repo mikevocalz/react-native-qnx, branch feat/worklets-reanimated-gesture-handler-qnx, PR 149 head 240dc0814dd129e806e5228d1b70b9cc27dbd2d6 (gh pr view: OPEN, draft). Shallow clone HEAD matched the full SHA.
- Mac clone made at scratchpad rnqnx-149b, tarred with COPYFILE_DISABLE=1 tar --no-xattrs --exclude=.git into /home/qnxbuild/pr149b/src. 0 AppleDouble files (find ._* = 0 before and after js/build). No source edits, so no patch file.
- Versions: react-native 0.88.1, worklets 0.13.1, reanimated 4.7.1, gesture-handler 3.4.0, react-native-svg 15.16.0, pnpm 12.18.0, node v26.10.0, hermesc 260318099.0.5.

## Flags (build.sh in /home/qnxbuild/pr149b, copied from pr149 with 3 edits)

cmake --preset qnx-aarch64-release (Release, -O2), plus:
-DRNQNX_BUILD_CONSUMER_APP=ON -DRNQNX_BUILD_REACT_QNX=ON -DRNQNX_BUILD_HERMES_QNX=ON -DRNQNX_FETCH_REACT_NATIVE_DEPS=ON
-DRNQNX_ENABLE_WINDOW_CHROME=ON -DRNQNX_ENABLE_INPUT_LIFECYCLE=ON
-DRNQNX_ENABLE_WORKLETS=ON -DRNQNX_ENABLE_REANIMATED=ON -DRNQNX_ENABLE_GESTURE_HANDLER=ON
-DRNQNX_ENABLE_QT_SVG=ON -DRNQNX_ENABLE_QT_RNSVG=ON
-DQt6Svg_DIR=/home/qnxbuild/qt-6.12.0-public/svg-qnx/lib/cmake/Qt6Svg
-DRNQNX_HERMES_QNX_DIR=/home/qnxbuild/hermes-qnx-full -DQt6_DIR=/home/qnxbuild/qt-6.12.0-public/qnx-build/qtbase/lib/cmake/Qt6 -DQT_HOST_PATH=/home/qnxbuild/qt-6.12.0-public/host-build/qtbase
-DRNQNX_ENABLE_HOST_SANITIZERS=OFF -DRNQNX_WARNINGS_AS_ERRORS=OFF, -DFETCHCONTENT_FULLY_DISCONNECTED=ON with the pr102-consumer _deps sources, CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -Wno-error=..." as in pr149/build.sh.
Edits to the pr149 recipe: paths pr149 -> pr149b; the three SVG flags added; build targets now rnqnx_qt_svg rnqnx_qt_rnsvg rnqnx_worklets rnqnx_reanimated rnqnx_gesture_handler rnqnx_consumer_app. The toolchain file copy gained one line: list(APPEND CMAKE_FIND_ROOT_PATH /home/qnxbuild/qt-6.12.0-public/svg-qnx).
CMakeCache confirms ON: RNQNX_ENABLE_QT_SVG, QT_RNSVG, WORKLETS, REANIMATED, GESTURE_HANDLER, WINDOW_CHROME, INPUT_LIFECYCLE.
QtSvg for QNX was already built (scripts/qt-public/build-svg-qnx.sh output): /home/qnxbuild/qt-6.12.0-public/svg-qnx (libQt6Svg.so.6.12.0 828968 B, plugins/imageformats/libqsvg.so 34816 B, dated 2026-11-13). Reused, not rebuilt.

## Commands, all in container qnx805-build, ninja -j2 (--parallel 2)

1. pnpm install --frozen-lockfile: exit 0
2. build.sh js (bundle-qnx, hermesc, prepare-native): exit 0
3. build.sh configure: exit 0
4. build.sh build: exit 0, ninja [723/723], 0 FAILED, 0 "error:", 21 "warning:" lines. No compile errors, so no fixes.

## Product

- qnx-build/consumer/qnx/org.rnqnx.showcase: ELF 64-bit LSB pie executable, ARM aarch64, interpreter /usr/lib/ldqnx-64.so.2, with debug_info, not stripped
- size 14056088 B, sha256 315cb1c34a600b4d48810476c4acec5b87fcea21e02243efc1f549ee6a303143
- NEEDED now includes libQt6Svg.so.6 (new vs pr149); also libhermesvm.so, libQt6Quick/OpenGL/Qml/Network/Gui/Core, libscreen, libGLESv2, libEGL.
- RPATH: container paths (qt qnx-build/qtbase/lib, svg-qnx/lib, hermes-qnx-full/lib).
- nm -C symbol counts (linked-in only, not a runtime test): worklets:: 2125; reanimated:: 5661; gesturehandler|rngh 476; Rnsvg|rnsvg|RNSVG 3813; QSvg|Qt6Svg|QtSvg|rnqnx_qt_svg 67.
- librnqnx_qt_rnsvg.a 1082262 B, librnqnx_qt_svg.a 191374 B.
- app.bundle.js (Hermes bytecode) sha256 358527886692ba802744dda0c6339b4713f5fde11c8ccbb270152596600204a5, identical to app.hbc.

## Deploy set: /work/pr149b-out (Mac /Users/mikevocalz/qnx-pi400-8.0.5/pr149b-out), 28 MB, SHA256SUMS inside lists all 21 files

org.rnqnx.showcase 315cb1c3...3143; app.bundle.js 35852788...04a5; libhermesvm.so a48934c6...6f62f (MinSizeRel full VM from hermes-qnx-full); native-app.json 15c656e8...74f4; libQt6Svg.so.6.12.0 9580a260...3283 (+ symlink libQt6Svg.so.6); plugins/imageformats/libqsvg.so 5a92e21d...a418; icons/icon-{16..512}.png.
Staged assets (layout from scripts/stage-gallery-assets.mjs --out): assets/assets/anim.gif c8387de6..., anim.webp b100308e..., icon.png cab067bf..., media/rnqnx-test-clip.mp4 b691da63..., media/rnqnx-test-tone.wav ab332856....
stage-gallery-assets.mjs --check /work/pr149b-out --app apps/component-gallery output: "all referenced assets are staged in /work/pr149b-out", exit 0.
Full hashes: see SHA256SUMS in the deploy dir.

## Not verified

- No run on QNX hardware or VM, no screenshot, no SVG render, gesture, animation or worklet test.
- libqsvg.so and libQt6Svg.so are the 2026-11-13 builds, not rebuilt here. Whether the Pi already has them, and where the host looks for the imageformats plugin (QT_PLUGIN_PATH), was not checked.
- Binary not stripped, not sized against a budget. 21 compiler warnings not triaged.
