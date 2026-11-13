# Image format plugins on the Pi 400, November 13, 2026

Question: does an animated GIF or WebP play through the `expo-image` view on the
Pi 400? An animated GIF did. An animated WebP did not, because no WebP plugin
existed in the Qt build. This receipt covers the cause, the new build step, and
the on-device proof. Base: `feat/expo-image-painted-view`, branch
`fix/pi-image-formats`.

## What the runtime and the app had

| Location                                               | Image plugins                                                                        |
| ------------------------------------------------------ | ------------------------------------------------------------------------------------ |
| `/data/home/qnxuser/qt612-public-o2/plugins`           | none; only `platforms/libqqnx.so` (`find ... -path '*imageformats*'` prints nothing) |
| `/data/home/qnxuser/rnqnx-tile-1/plugins/imageformats` | `libqgif.so`, `libqjpeg.so`                                                          |

The release launcher (`rnqnx-tile-1/run-app.sh`) sets
`QT_PLUGIN_PATH=$APP_DIR/plugins:$QT_ROOT/plugins`, so the app directory is the
only source of image plugins.

A copy of the `rnqnx-tile-1` app directory, launched as
`org.rnqnx.showcase --window-size 800x600 --screenshot <png>` with
`QT_DEBUG_PLUGINS=1` (`run-app-plugin-load.sh`), loaded exactly the plugins in
its `plugins/imageformats`: `libqgif.so` and `libqjpeg.so` in variant A; those
two plus `libqicns`, `libqtga`, `libqtiff`, `libqwbmp` and `libqwebp` in variant
B (`qt.core.library: ... loaded library` lines). `rnqnx-tile-1` itself was not
modified.

`QImageReader::supportedImageFormats()` on the board, with the plugin
directories above (`probe-run-pi400.txt`):

| Plugins                    | Formats                                                                     |
| -------------------------- | --------------------------------------------------------------------------- |
| A: app GIF + JPEG, runtime | `bmp gif jfif jpeg jpg pbm pgm png ppm xbm xpm`                             |
| B: A + qtimageformats      | `bmp gif icns jfif jpeg jpg pbm pgm png ppm tga tif tiff wbmp webp xbm xpm` |

So gif, png and jpeg work today; webp is missing in A. `svg` is in neither
list: SVG decoding is `libqsvg.so` from the `qtsvg` module, which this Qt build
does not include (`configure-qnx.sh` builds `qtbase`, `qtshadertools` and
`qtdeclarative`). SVG is not part of this change.

## Where plugins come from

PR #145 (`ee272fe`, "Load images by URL on QNX devices") added `QJpegPlugin` and
`QGifPlugin` to the `configure-qnx.sh` target list and the two `.so` files to
`PLUGINS` in `scripts/qt-public/stage-runtime.py`. qtbase builds only the GIF,
ICO and JPEG image plugins. The `qtimageformats` module (WebP, TIFF, TGA, ICNS,
WBMP, plus JP2 and MNG that need system libraries) was never extracted:
`extract-source.sh` listed three submodules, and the `-submodules` argument of
`configure-qnx.sh` names three. Nothing in the repository copies plugins into an
app directory; the dev-client doc lists them as a manual deploy step.

## Build

`scripts/qt-public/build-imageformats-qnx.sh` (new) extracts `qtimageformats`
from the hash-checked 6.12.0 archive and builds it against the existing QNX
qtbase with `qt-configure-module`, in `qnx805-build`, `JOBS=2`. It configures
bundled libwebp and libtiff, checks for `-O2`, and builds `QWebpPlugin`,
`QTiffPlugin`, `QTgaPlugin`, `QICNSPlugin` and `QWbmpPlugin`. Run from the
repository copy into `/home/qnxbuild/fmt-pr/script-build`: 201 steps, exit 0.
An earlier manual build with the same options in `/home/qnxbuild/fmt-pr/build`
produced byte-identical files (same SHA-256 for all five).

Configure summary: `WEBP yes, Using system libwebp no`, `TIFF yes, Using system
libtiff no`, `JasPer no`, `MNG no`. `libqwebp.so` needs only `libQt6Gui`,
`libQt6Core`, `libGLESv2`, `libEGL`, `libc++`, `libm`, `libc` and `libgcc_s`,
all already on the board.

| File          | SHA-256                                                            |
| ------------- | ------------------------------------------------------------------ |
| `libqwebp.so` | `98caf99ec812055ad0b12b3b38eac9d2d18f2da1478e9a0a7d311a0704d3eae1` |
| `libqtiff.so` | `001af5139e281b9edd61857619edac643c3a66d2f453d190e42144637c11aa1d` |
| `libqtga.so`  | `abe209a1d6cd123df8490ac933d620df7d4edca52e64fbae42d208d1cc46198e` |
| `libqicns.so` | `07929b560161d67e0475e0cc5edabfe3d9ceb95264ec95c42c40f5018c273807` |
| `libqwbmp.so` | `4decee3cd40facc80c7975d6caba972b8f800759e33b133306140eeecc2ab45e` |

`stage-runtime.py` now stages the five files into `plugins/imageformats/` and
adds the `qtimageformats` license texts. A test run against a scratch tree
(`/home/qnxbuild/fmt-pr/runtime`) completed with every staged ELF's `DT_NEEDED`
closure resolved, and `plugins/imageformats/` held the seven files. That run
used no SPDX audit file; the audit of the `imageformats-qnx` tree was not run.

## Decode proof on the Pi 400

`scripts/qt-public/probes/image-formats/probe.cpp`, cross-built in the
container, ran from `/data/home/qnxuser/rnqnx-fmt-1` under the board's Qt
runtime with the QNX platform plugin (`run-probe.sh`; the probe needs a
`QGuiApplication` because `QMovie` stores frames as `QPixmap`). Test files:
`anim.gif` and `anim.webp`, four 64x48 frames (red, green, blue, yellow), 100 ms
each, made with Pillow.

With the app's GIF and JPEG plugins only (variant A), `anim.gif` decodes
(`imageCount=4`) and `anim.webp`, `still.webp` and `still.tiff` report
`format= size=-1x-1 imageCount=-1` and `QMovie valid=0`.

With the new plugins (variant B), `anim.gif` and `anim.webp` give identical
results:

```
anim.webp: format=webp size=64x48 supportsAnimation=1 imageCount=4
  reader frame 0..3: px(2,2)=ffff0000 ff00ff00 ff0000ff ffffff00, delay=100ms
  QMovie valid=1 frameCount=4
  frameChanged 0: px(2,2)=ffff0000
  frameChanged 1: px(2,2)=ff00ff00
  frameChanged 2: px(2,2)=ff0000ff
  frameChanged 3: px(2,2)=ffffff00
  frameChanged 0: px(2,2)=ffff0000
```

The `QMovie` half uses the same calls as `ExpoImageItem::adoptMovie`: a
`QBuffer` over the bytes, `CacheNone`, `start()`, then `currentImage()` on each
`frameChanged`. Four distinct frames arrived in 450 ms and the loop restarted.
`still.webp` and `still.tiff` decode as single frames. Full output:
`pi-image-formats-2026-11-13/probe-run-pi400.txt`.

## How `ExpoImageItem` plays an animation

It already handles animations; no code change is needed for GIF or WebP once the
plugin is in the app directory.

- `vnext/src/views/qt/image/ImageDecode.cpp:34` sets
  `animated = reader.supportsAnimation() && reader.imageCount() > 1`, skips
  downscaling for animated images (line 35) and keeps the encoded bytes (line 55).
- `ExpoImageItem.cpp:485-499` (`adoptMovie`) builds a `QMovie` over a `QBuffer`,
  and on `frameChanged` clears the layer cache key and calls `update()`.
- `ExpoImageItem.cpp:67` (`Layer::frame()`) returns `movie->currentImage()`, and
  the render cache key (line 563) includes `currentFrameNumber()`, so each frame
  repaints.
- `show()` starts the movie when `autoplay` is set (line 508);
  `startAnimating()` and `stopAnimating()` map to `start()` and `setPaused()`.

The one dependency is the plugin: with no `libqwebp.so`, `reader.size()` is
invalid and `decodeImage` returns `Invalid image or image exceeds pixel limit`
for every WebP, still or animated.

## Deploying

Copy `libqwebp.so` (and the other four if wanted) from the build into
`<app>/plugins/imageformats/`; the release launchers already put
`$APP_DIR/plugins` first on `QT_PLUGIN_PATH`. Staged runtimes built from this
branch carry them in `plugins/imageformats/` as well.

## Device state

`/data/home/qnxuser/rnqnx-fmt-1` holds the probe, test files, new plugins, a
copy of the tile-1 app directory (`appcopy`) and logs. No app window was left
open (both `--screenshot` runs exited 0). `rnqnx-tile-1` was read only.
