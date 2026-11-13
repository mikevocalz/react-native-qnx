# Qt Multimedia add-on for QNX aarch64le

Builds FFmpeg 8.0 (LGPL, shared) and the public-source Qt Multimedia 6.12.0
module as an add-on to the public-source Qt 6.12.0 runtime from
`scripts/qt-public/`. The Qt core build is not touched: nothing here rebuilds
or overwrites `qt612-public-o2`. The add-on installs into its own directory,
`/data/home/qnxuser/qt612-media/`, and `run-media-app.sh` points a media app at
it.

Run inside the `qnx805-build` container, after `scripts/qt-public/` has
produced the Qt build tree (`$QT_PUBLIC_BUILD/qnx-build`).

| Step | Script | Output |
| ---- | ------ | ------ |
| 1 | `build-ffmpeg.sh` | `$MEDIA_ROOT/ff-stage` (headers, `libav*.so`) |
| 2 | `build-qtmultimedia.sh` | `$MEDIA_ROOT/qtmm-stage` (`libQt6Multimedia*`, QML module, `ffmpeg` plugin) |
| 3 | `stage-media-runtime.sh` | `$MEDIA_ROOT/runtime` + `SHA256SUMS`, ready to copy to the board |

Sources and hashes are listed in `../../docs/packaging.md` and
`../../docs/evidence/media-2026-11-13.md`.
