# Public-source Qt 6.12.0 for QNX aarch64le

These scripts rebuild the Qt runtime recorded in
[the Pi 400 receipt](../../docs/evidence/pi400-public-qt-2026-11-12-b51b49c.md).
They run in a Linux container with QNX SDP 8.0.5 installed (the
`qnx805-build` container used on 2026-11-12, user `qnxbuild`). The QNX SDP is
licensed separately and is not part of this repository.

## Layout

[`layout.sh`](layout.sh) sets every path; override with environment
variables.

| Variable          | Default                           | Holds                                                              |
| ----------------- | --------------------------------- | ------------------------------------------------------------------ |
| `QT_PUBLIC_ROOT`  | `/work/qt-6.12.0-public`          | `download/` (archive, `cve-patches/`), `src/`, `runtime/`, `logs/` |
| `QT_PUBLIC_BUILD` | `/home/qnxbuild/qt-6.12.0-public` | `host-build/`, `qnx-build/`, `qnx-stage/`                          |
| `QNX_SDP`         | `/work/qnx800`                    | `qnxsdp-env.sh`                                                    |
| `GUARD_PATH`      | `/work`                           | Mount whose free space `guarded-run.sh` watches                    |

`/work` was the host's `~/qnx-pi400-8.0.5` mounted into the container. Put
the downloads in place first:

- `$QT_PUBLIC_ROOT/download/qt-everywhere-src-6.12.0.tar.xz` from
  `https://download.qt.io/official_releases/qt/6.12/6.12.0/single/`
  (SHA-256 `98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165`).
- `$QT_PUBLIC_ROOT/download/cve-patches/` with
  `CVE-2026-116337-qtbase-6.12-0001.diff`, `...-0002.diff` and
  `CVE-2026-105924-qtbase-6.12.diff` from
  `https://download.qt.io/official_releases/qt/6.12/`.

## Order

Run from this directory inside the container:

```sh
source ./layout.sh                  # exports the paths used below
./extract-source.sh                 # checks the archive hash, extracts 3 submodules
./apply-cve-patches.sh              # checks patch hashes, applies src/ hunks once
./apply-qnx-patches.sh              # applies patches/*.patch to qtbase once
MIN_KB=4194304 ./guarded-run.sh ./configure-host.sh "$QT_PUBLIC_ROOT/logs/host-build.log"
MIN_KB=4194304 JOBS=4 ./guarded-run.sh ./configure-qnx.sh "$QT_PUBLIC_ROOT/logs/qnx-build.log"
source "$QNX_SDP/qnxsdp-env.sh"
python3 -I spdx-audit.py "$QT_PUBLIC_BUILD/qnx-build" "$QT_PUBLIC_SRC" \
  "$QT_PUBLIC_ROOT/runtime-spdx-audit.json" Core Network Gui OpenGL Qml QmlMeta \
  QmlModels QmlWorkerScript Quick QuickShapes QQnxIntegrationPlugin qmlplugin \
  modelsplugin workerscriptplugin qtquick2plugin qmlshapesplugin
python3 -I stage-runtime.py "$QT_PUBLIC_ROOT/runtime" "$QT_PUBLIC_ROOT/runtime-spdx-audit.json"
```

`stage-runtime.py` writes `manifest.json` and
`needed.json`; the receipt's `SHA256SUMS` was then written with
`shasum -a 256` over every staged file.

Qt is compiled at `-O2` ([ADR-0017](../../docs/adr/0017-qt-qnx-optimization-level.md)).
`configure-qnx.sh` no longer passes `-optimize-size`, sets
`FEATURE_optimize_size` and `FEATURE_optimize_full` off, and exits after
configure unless `CMAKE_C_FLAGS_RELEASE` and `CMAKE_CXX_FLAGS_RELEASE` contain
`-O2`. The 2026-11-12 runtime in the receipt predates this and was compiled
with no `-O` level. QtCore and QtGui still compile at `-O3`: Qt applies
`qt_internal_add_optimize_full_flags()` to those two modules whenever
`optimize_size` is off. The first `-O2` build is in
[the -O2 receipt](../../docs/evidence/pi400-public-qt-o2-2026-11-12-a79e16a.md).

To rebuild an existing `qnx-build` tree in place, apply the patches to the
existing source first (each patch script refuses to run twice), move
`qnx-build/CMakeCache.txt` aside and run `configure-qnx.sh` again. The
2026-11-12 `-O2` rebuild did this; the tree went from 897 MB at `-O0` to
355 MB, so no second tree is needed. Write the runtime tarball with
`tar --no-xattrs --no-mac-metadata` on macOS, or it carries
`LIBARCHIVE.xattr` headers that QNX `tar` warns about.

## QNX platform plugin patch

[`patches/qtbase-qnx-offscreen-surface.patch`](patches/qtbase-qnx-offscreen-surface.patch)
targets the black window Qt Quick's OpenGL path shows under
`fullscreen-winmgr` on the Pi 400. Unpatched, the window stays black there, while the same app renders with
`QT_QUICK_BACKEND=software`, renders with OpenGL when no window manager runs,
and `grabWindow()` returns the right pixels.

The cause: for every OpenGL-rendered `QQuickWindow`, Qt Quick asks for a
`QOffscreenSurface` (`QSGRhiSupport::maybeCreateOffscreenSurface`, then
`QRhiGles2InitParams::newFallbackSurface`). The stock qnx plugin does not
implement `createPlatformOffscreenSurface()`, so `QOffscreenSurface::create()`
falls back to a hidden `QWindow`. That hidden window still becomes a top-level
libscreen application window (1x1, created right after the real one), and
`fullscreen-winmgr` manages it like any other app window. With
`QT_LOGGING_RULES="qt.qpa.*=true"` the unpatched run logs
`QWindow(..., name="QOffscreenSurface")` being created and added to the
screen, followed by two `SCREEN_PROPERTY_VISIBLE` (51) events on the real
window; the software backend creates no such window.

The patch implements `createPlatformOffscreenSurface()` with `QEGLPbuffer`,
as eglfs does: a surfaceless context when `EGL_KHR_surfaceless_context` is
available (Mesa on the Pi has it), otherwise a pbuffer. No screen window is
created. `QQnxGLContext::eglSurfaceForPlatformSurface()` returns the pbuffer
for offscreen surfaces instead of casting them to `QQnxEglWindow`. The patched
run logs `createPlatformOffscreenSurface ... valid = true` and no
`QOffscreenSurface` window.

To rebuild only the plugin, apply the patch and build the
`QQnxIntegrationPlugin` target; `libqqnx.so` is the only changed file. When
deploying it beside the stock runtime, point both `QT_QPA_PLATFORM_PLUGIN_PATH`
and `QT_PLUGIN_PATH` at the new plugin tree: with `QT_PLUGIN_PATH` still on the
stock `plugins/`, Qt loaded the stock `platforms/libqqnx.so` even though
`QT_QPA_PLATFORM_PLUGIN_PATH` named the patched one.

## Resizing a window under a window manager

[`patches/qtbase-qnx-resize-swap.patch`](patches/qtbase-qnx-resize-swap.patch)
applies after the offscreen-surface patch and fixes two faults that showed
when an app resized its own window under `rnqnx-winmgr`
(`tools/qnx-winmgr`):

1. **A lost frame at the start of every resize.** Each resize gesture logged
   `QEGLPlatformContext: eglSwapBuffers failed: 300d` (`EGL_BAD_SURFACE`), with
   the threaded and the basic render loop alike: six failed swaps in six
   corner drags on the Pi 400. `QEGLPlatformContext::swapBuffers()` looks the
   surface up again through `eglSurfaceForPlatformSurface()`, and the qnx
   plugin applies a resize requested since `makeCurrent()` there: it releases
   the context, destroys the bound surface and creates a new one that is not
   current, then swaps it. The patched `QQnxGLContext::swapBuffers()` swaps
   the surface `makeCurrent()` bound and leaves the resize to the next
   `makeCurrent()`.
2. **A stretched frame while resizing.** `setGeometry()` wrote the new
   `SCREEN_PROPERTY_SIZE` and `SOURCE_SIZE` straight away, while the buffers
   kept the old size until the next frame, so Screen scaled the last frame
   to the new size. The patched `QQnxEglWindow` leaves the Screen window size
   alone until buffers of the new size exist and sets it just before the swap
   that posts the first frame drawn into them, so the size and the pixels
   arrive in one post.
3. **A shifted frame on a left or top edge resize.** Such a resize moves the
   window and changes its size. With only the size deferred, `setGeometry()`
   still wrote `SCREEN_PROPERTY_POSITION` at once, so for one frame the old
   pixels showed at the new place and the right (or bottom) edge jumped by the
   drag distance. On the Pi 400, `rnqnx-winmgr -v` logged the window's edges
   at each property change of a left edge drag: the right edge read 1960 and
   2000 between frames instead of 1920. The patch defers the position with
   the size and writes both just before that swap. A pure move, with no size
   change, is applied at once; a move made while a resized frame is still
   unposted waits for that frame. After the patch the right edge stayed at
   1599 for every change in a 10-step left edge drag, and the bottom edge at
   939 in a top edge drag.

After the patch the same six drags logged no failed swap, and a screenshot
taken during a slow drag showed the title bar and content drawn at the
window's size.

Rebuilding the plugin recompiles every source in `platforms/qnx`, because
the patch adds a member to `QQnxWindow`; mixing old and new objects breaks
the class layout.

## On the board

`public-qt-env.sh` and the `run-public-*.sh` launchers expect the runtime at
`/data/home/qnxuser/qt612-public` and the proof directory at
`/data/home/qnxuser/qt612-public-proofs`. They do not touch boot files.

## QtSvg for SVG images

`build-svg-qnx.sh` builds qtsvg (`libQt6Svg.so`, `plugins/imageformats/libqsvg.so`)
against the QNX qtbase with `qt-configure-module`, after `configure-qnx.sh`:

```sh
./build-svg-qnx.sh        # output in $QT_PUBLIC_BUILD/svg-qnx
python3 stage-runtime.py  # stages both files when the build exists
```

Qt Quick and `QImageReader` find the plugin through `QT_PLUGIN_PATH`.
