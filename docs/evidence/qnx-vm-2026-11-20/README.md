# QNX 8.0.5 QEMU VM captures, 2026-11-20

**This is QEMU VM evidence. It is not Pi 400 evidence and it does not make any
Pi 400 or Pi 4 QSTI row device-verified.** The Pi 400 was offline when these
captures were taken (192.168.2.2 unreachable). `docs/component-support.json`
keeps this evidence in a separate `vm` field.

## What ran

| Item               | Value                                                                                                                                                                 |
| ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Board              | QEMU 11.1.3 `-machine virt,gic-version=3`, TCG, `-cpu cortex-a57`, 4 CPUs, 3 GiB RAM. HVF was tried first and QNX's GICv3 init asserts under it (see the VM runbook). |
| Host               | Apple M3 Pro, macOS 27.0 (Darwin 27.0.0), QEMU from Homebrew                                                                                                          |
| Guest              | `QNX qnxvm 8.0.0 2026/06/05-16:21:14EDT QEMU_virt aarch64le` ([uname and pidin](vm/uname-pidin.txt))                                                                  |
| Image              | `mkqnximage --type=qemu --arch=aarch64le --graphics=no`, SDP 8.0.5 plus `qemuvirt`, `driver.virtio.*` and `screen.board.null`. `ifs.bin` sha256 in `vm/`.             |
| Display stack      | QNX Screen with `screen-sw.so` and the dummy WFD driver (`libWFDdummy`, `libwfdcfg-dummy`), mode 1280 x 800. No GPU, no EGL driver.                                   |
| Qt                 | Public-source Qt 6.12.0 for QNX, `QT_QPA_PLATFORM=qnx:no-fullscreen`, `QT_QUICK_BACKEND=software`, with the patch below                                               |
| Window             | `--fullscreen` on the 1280 x 800 Screen display, 1280 x 800 PNG per capture                                                                                           |
| Native host build  | Commit `746f52d` (main after PR #188), `qnx-aarch64-release` preset, exact flags in `vm/build.sh`. `org.rnqnx.showcase` sha256 in `vm/deploy.sha256`.                 |
| JavaScript bundles | Built from this PR's first commit `c3787a0` (the twelve cards and the proof entry). Hashes in `vm/proof-bundles.sha256`.                                              |
| Capture            | The app's own `--screenshot` option (`QQuickWindow::grabWindow`), not the QNX `screenshot` tool, which is not in this SDP                                             |

The Qt `qnx` platform plugin needed two changes to run without a GPU, in
`scripts/qt-public/patches/qtbase-qnx-software-grab.patch`:

1. `QQnxRasterBackingStore` had no `toImage()`, so `grabWindow()` returned a
   null image with the software scene graph. That is why the Pi 400 guide says
   `--screenshot` returns no image in software mode. The patch returns a copy
   of the current render buffer.
2. `QQnxIntegration::createEglDisplay()` aborted when no EGL display exists.
   With `QT_QUICK_BACKEND=software` it now warns and continues.

The plugin used here is the patched build. Its sha256 is in `vm/deploy.sha256`
(`./qt/plugins/platforms/libqqnx.so`).

## Commands

Build (container `qnx805-build`, SDP 8.0.5, `--parallel 4`). The full command
line is `vm/build.sh` with `vm/qnx-qt-public.toolchain.cmake`; it ran as
`build.sh js`, `build.sh configure`, `build.sh build` in a fresh directory
(`pnpm install --frozen-lockfile` first). 723 steps, exit 0.

Bundles:

```sh
node packages/react-native-qnx-cli/src/bin.mjs bundle-qnx --app component-gallery --out build/gallery/app.bundle.js
scripts/vm-proof/build-bundles.sh <out> <hermesc> virtualized-list section-list virtualized-section-list \
  button touchable-opacity touchable-highlight touchable-without-feedback image-background \
  keyboard-avoiding-view refresh-control safe-area-view status-bar
```

On the VM (`vm/capture-run.txt` is the output):

```sh
/data/vm/capture.sh /data/vm/app /data/vm/bundles /data/vm/shots <card-id>...
# each card runs: ./org.rnqnx.showcase --fullscreen --bundle proof-<id>.hbc --screenshot <id>.png
```

Boot: `vm/run.sh`. Per-run stderr is in `logs/`. Hashes are in `SHA256SUMS.txt`.

## Results

Every capture is a one-shot render of one card. Nothing was pressed, scrolled
or typed, so no capture shows interaction. Each card resolves the component
from `import { X } from 'react-native'` first and prints the error when that
fails, then falls back to a deep import so the component itself is still shown.

| Component                  | Capture                                                                   | Result                                                                                                                                                                                    |
| -------------------------- | ------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `VirtualizedList`          | [virtualized-list.png](captures/virtualized-list.png)                     | **Public entry fails**: `React Native QNX public export "VirtualizedList" is not implemented in this experimental entry.` Deep import renders 200 rows, 5 visible. Issue #178 still true. |
| `SectionList`              | [section-list.png](captures/section-list.png)                             | Public entry works. Three sections, headers and separators render; the first five rows are visible.                                                                                       |
| `VirtualizedSectionList`   | [virtualized-section-list.png](captures/virtualized-section-list.png)     | **Public entry fails** with the same error. Deep import renders the Alpha and Beta sections. Issue #178 still true.                                                                       |
| `Button`                   | [button.png](captures/button.png)                                         | **Public entry fails** (`"Button" is not implemented`). Deep import renders an enabled and a disabled button. Not covered by #178.                                                        |
| `TouchableOpacity`         | [touchable-opacity.png](captures/touchable-opacity.png)                   | Public entry works. Child renders; press count 0. Dimming on press not exercised.                                                                                                         |
| `TouchableHighlight`       | [touchable-highlight.png](captures/touchable-highlight.png)               | Public entry works. Child renders; press count 0. Underlay not exercised.                                                                                                                 |
| `TouchableWithoutFeedback` | [touchable-without-feedback.png](captures/touchable-without-feedback.png) | Public entry works. Child renders; press count 0.                                                                                                                                         |
| `ImageBackground`          | [image-background.png](captures/image-background.png)                     | **Public entry fails** (`"ImageBackground" is not implemented`). Deep import paints the bundled image behind the child with `resizeMode` cover.                                           |
| `KeyboardAvoidingView`     | [keyboard-avoiding-view.png](captures/keyboard-avoiding-view.png)         | Public entry works. Children and a `TextInput` render inside it. No avoidance is possible without a Keyboard module, and none was tested.                                                 |
| `RefreshControl`           | [refresh-control.png](captures/refresh-control.png)                       | Public entry works. The `ScrollView` renders rows 1 to 4; no refresh indicator is drawn, as the support table says. Pull gesture not exercised.                                           |
| `SafeAreaView`             | [safe-area-view.png](captures/safe-area-view.png)                         | **Public entry fails** (`"SafeAreaView" is not implemented`). Deep import renders a View-like box; no inset is visible.                                                                   |
| `StatusBar`                | [status-bar.png](captures/status-bar.png)                                 | Public entry works. The element mounts and draws nothing.                                                                                                                                 |

Also captured: [gallery.png](captures/gallery.png), the full gallery's first
screen from the same build.

Five of the twelve components are missing from the public entry, not two. The
issue #178 text names `VirtualizedList` and `VirtualizedSectionList`;
`Button`, `ImageBackground` and `SafeAreaView` fail the same way. An app that
imports any of them from `react-native` throws on QNX. This was not fixed here.

Follow-up, host only: the public entry now exports all five
(`packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js`), and
`tests/js/qnx-public-entry-render.test.mjs` mounts each through it. The captures
and results in this directory were taken before that change and are unchanged.
VM re-capture of the fixed entry: not run, the drive holding the VM image was
offline.

## Other things the logs show

- The gallery logs `Could not parse yoga::PositionType: left` once
  ([gallery.log](logs/gallery.log)).
- The remote image card fails with an SSL handshake error. The VM has no CA
  bundle deployed and its only network is QEMU user-mode NAT.
- The first run of `image-background` on an earlier set of bundles drew no
  image because the bundles sat in a directory without the staged `assets/`
  tree (`QML Image: Cannot open`). The final run has `assets` linked next to
  the bundles.

## Not shown

No pointer or keyboard input, no scrolling, no GPU rendering, no timing that
means anything (TCG on an M3 Pro), and nothing about the Pi 400's Screen driver
or its V3D GPU path.
