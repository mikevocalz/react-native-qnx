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

The Release configuration carries no `-O` level, as in the evaluation build;
see the receipt.

## On the board

`public-qt-env.sh` and the `run-public-*.sh` launchers expect the runtime at
`/data/home/qnxuser/qt612-public` and the proof directory at
`/data/home/qnxuser/qt612-public-proofs`. They do not touch boot files.
