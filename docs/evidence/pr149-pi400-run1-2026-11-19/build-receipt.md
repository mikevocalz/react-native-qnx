# PR 149 QNX aarch64 cross-build receipt (2026-11-19)

Scope: cross-compile result only. This is NOT device acceptance. Nothing was run on the Pi (192.168.2.2), nothing was pushed, merged or edited on GitHub.

## Source

- Repo: github.com/mikevocalz/react-native-qnx, branch feat/worklets-reanimated-gesture-handler-qnx (PR 149 head, contains 159 and 161)
- Commit built: 26bd6aa238b7ee535e73e75d79017749b64f22e3 (Mac checkout clean, `git status --porcelain` empty)
- Copied with tar (excluding .git, node_modules, build) into /home/qnxbuild/pr149/src. No repo source was modified, so there is no patch file.
- Copy artifact: macOS tar added 2952 AppleDouble `._*` files; the first Metro bundle failed on one of them (`app/(tabs)/.__layout.js`). They were deleted from the container copy only, the stale build dir from the failed configure was removed, and the steps were rerun.

## Installed package versions (pnpm install --frozen-lockfile, pnpm 12.18.0, node v26.10.0)

- react-native 0.88.1
- react-native-worklets 0.13.1
- react-native-reanimated 4.7.1
- react-native-gesture-handler 3.4.0
- Hermes compiler (hermesc) 260318099.0.5

## Toolchain

- QNX SDP 8.0.5 aarch64 (/work/qnx800), CMake 3.31.9, Ninja 1.11.1, Qt 6.12.0 public build, preset qnx-aarch64-release (CMAKE_BUILD_TYPE=Release, -O2)
- Hermes: -DRNQNX_HERMES_QNX_DIR=/home/qnxbuild/hermes-qnx-full. RnqnxHermes (cmake/dependencies/HermesQNX.cmake) needs lib/libhermesvm.so when Worklets is ON, and /work/pr102-candidate/build/hermes-qnx-aarch64 only has libhermesvmlean.so, so the dev-recipe Hermes (MinSizeRel, full VM) was used.

## Commands (all inside container qnx805-build; script /home/qnxbuild/pr149/build.sh)

1. `pnpm install --frozen-lockfile` in /home/qnxbuild/pr149/src: exit 0 ("Lockfile is up to date", +581 packages)
2. `./build.sh js` (bundle-qnx, hermesc -O -emit-binary, project prepare-native): exit 0 (second run, after AppleDouble cleanup)
3. `./build.sh configure` (cmake --preset qnx-aarch64-release with -DRNQNX_ENABLE_WORKLETS=ON -DRNQNX_ENABLE_REANIMATED=ON -DRNQNX_ENABLE_GESTURE_HANDLER=ON, plus the devx3 recipe flags, FETCHCONTENT_FULLY_DISCONNECTED=ON): exit 0. CMakeCache confirms all three switches ON.
4. `./build.sh build` = `cmake --build qnx-build --target rnqnx_worklets rnqnx_reanimated rnqnx_gesture_handler rnqnx_consumer_app --parallel 2 -- -k 0`: exit 0

Ninja: [633/633], 0 FAILED lines, 0 `error:` lines, 21 `warning:` lines in the log. No repo source change and no header adapter were needed (the adapters from docs/validation/pr149-on-native-build-2026-11-14.md are already in the tree).

## Product

- /home/qnxbuild/pr149/qnx-build/consumer/qnx/org.rnqnx.showcase/org.rnqnx.showcase
- file(1): ELF 64-bit LSB pie executable, ARM aarch64, version 1 (SYSV), dynamically linked, interpreter /usr/lib/ldqnx-64.so.2, with debug_info, not stripped
- size 11994472 bytes, sha256 942c64cea3fc603be2e2343fcac557a5f132f2639c7e57aa8375ecf526726419
- NEEDED: libhermesvm.so, libscreen.so.1, libQt6Quick/OpenGL/Gui/Qml/Network/Core.so.6, libGLESv2.so.1, libEGL.so.1, libsocket.so.4, libc++.so.2, libm.so.3, libc.so.6, libgcc_s.so.1
- RPATH is the container paths (/home/qnxbuild/hermes-qnx-full/lib, /home/qnxbuild/qt-6.12.0-public/qnx-build/qtbase/lib); on a device the libraries must be found through the device's own layout or LD_LIBRARY_PATH.
- Linked-in symbol counts from `nm -C` (not a runtime test): worklets:: 2125, reanimated:: 5655, gesturehandler/rngh 476.
- Static libraries: librnqnx_worklets.a 19934844 B, librnqnx_reanimated.a 26434766 B, librnqnx_gesture_handler.a 17663772 B.
- Bundle: app.bundle.js is Hermes bytecode, sha256 358527886692ba802744dda0c6339b4713f5fde11c8ccbb270152596600204a5 (identical to apps/component-gallery/build/qnx/app.hbc).

## Pi deploy set: /work/pr149-out (Mac: /Users/mikevocalz/qnx-pi400-8.0.5/pr149-out), 25 MB

sha256
942c64cea3fc603be2e2343fcac557a5f132f2639c7e57aa8375ecf526726419 org.rnqnx.showcase
358527886692ba802744dda0c6339b4713f5fde11c8ccbb270152596600204a5 app.bundle.js
a48934c6560d8da5adeb7fadb7469e4c02b9dd925846fdbb69bf36f78f96f62f libhermesvm.so (from hermes-qnx-full, MinSizeRel)
955c7cb86b8ae9eeb68993919703cd7891e512d9733295c7400d90975316f217 native-app.json
icons/icon-{16,24,32,48,64,96,128,192,256,512}.png (hashes in the report)

## Not verified

- No run on QNX hardware or a QNX VM, no screenshot, no gesture, animation or worklet execution test.
- The binary was not stripped or sized against a budget.
- Which libhermesvm.so the Pi already has was not checked; the one in the deploy set is the build the binary was linked against.
