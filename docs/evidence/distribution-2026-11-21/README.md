# Distribution gate: Qt source build and closure, 2026-11-21

**The `distribution` gate stays pending.** This receipt re-checks the public Qt
source identity, reproduces the source preparation and the configure step,
builds the qtimageformats plugins, stages a runtime for the current closure with
notices and hashes, checks it with a new script, and finds and works around one
defect in how the closure resolves libraries. It does not rebuild Qt's core
libraries, and it does not produce a release package or a source offer. Every result below is labelled **host**, **container**
(`qnx805-build`, Linux aarch64 on Colima, QNX SDP 8.0.5 `qcc`) or **QNX VM**
(QEMU TCG, software render). Nothing here is Pi 400 or release-board evidence,
and the gate does not need a device (`TODO.md`: "Needs a device: No").

Repository base: `fb20c3ad` (main after PR #205). Hashes of every file in this
folder are in [SHA256SUMS.txt](SHA256SUMS.txt).

## What the gate requires

From `docs/releases/v1-readiness.json`, `distribution`: "Qualify the eligible
public Qt source build, exact module/QPA notices and hashes,
shared-library/plugin closure and applicable source/library replacement
materials. Keep evaluation and QNX SDK payloads separate. No paid Qt
subscription prerequisite." `TODO.md` adds: build Qt from the public source
archive and record module/QPA notices, hashes and the library/plugin closure;
define package manifests and release units. [ADR-0013](../../adr/0013-open-source-qt.md)
and [packaging](../../packaging.md) say what a package must carry.

| #   | Requirement                                                    | State after this receipt                                                                                                                                                                                                                                                                                                                |
| --- | -------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | Eligible public Qt source build                                | Source identity re-verified; fresh extraction, patching and configure reproduced; qtimageformats built from the fresh tree (below). The Qt core libraries were not rebuilt, so a clean end-to-end build is **open**.                                                                                                                    |
| 2   | Exact module and QPA notices and hashes                        | Met for the current closure as staged here: 208 entries, 188 hashed files, SPDX audits for qtbase, qtsvg and qtimageformats, `THIRD_PARTY_ATTRIBUTIONS.txt` ([runtime-current](runtime-current/)). It is built from the 2026-11-12 libraries plus today's plugins, so it is not yet a single-pass build: **open** for that reason only. |
| 3   | Shared-library and plugin closure                              | `verify-closure.py` passes every check on the staged current runtime after the RPATH rewrite; all 26 ELF files load on the QNX VM (below). The build still writes the RPATH, so this holds only with the post-link step: **open** until the build is fixed.                                                                             |
| 4   | Applicable source and library replacement materials            | Replacement works on the QNX VM for the gallery app after the RPATH rewrite (below). The written source offer and a source bundle are not assembled. **Open**.                                                                                                                                                                          |
| 5   | Evaluation and QNX SDK payloads kept separate                  | `verify-closure.py` fails a runtime that holds a QNX system library. The `-O2` runtime passes; the VM-deployed tree fails because it carries `libeventfd.so.1` for the VM. No evaluation artifact is in this folder.                                                                                                                    |
| 6   | No paid Qt subscription prerequisite                           | Met by ADR-0013; nothing here needed one.                                                                                                                                                                                                                                                                                               |
| 7   | Package manifests and release units (`TODO.md`, G0 carry-over) | Not done. No package has been produced.                                                                                                                                                                                                                                                                                                 |

## Source identity (host)

[source/archive-and-patch-hashes.txt](source/archive-and-patch-hashes.txt)

- The local `qt-everywhere-src-6.12.0.tar.xz` hashes to
  `98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165`. That
  equals the `.sha256` file fetched from `download.qt.io` today
  ([source/qt-everywhere-src-6.12.0.tar.xz.sha256](source/qt-everywhere-src-6.12.0.tar.xz.sha256)),
  the `Digest: SHA-256` response header decoded from base64, and the committed
  value in `extract-source.sh`. MD5 `ac677cc64158740f257fd37a5b4c5ffb` equals
  [source/md5sums.txt](source/md5sums.txt).
- `official_releases/qt/6.12/` still lists only `6.12.0/`, no 6.12.1
  ([listing rows](source/upstream-6.12-diffs.txt)).
- The three applied qtbase diffs were downloaded again and match the hashes in
  `apply-cve-patches.sh`.
- **Finding:** the same listing has a fourth qtbase diff,
  [`CVE-2026-105943-qtbase-6.12.diff`](source/CVE-2026-105943-qtbase-6.12.diff),
  SHA-256 `dd22629ce948cf0a900421323cca331907180242a9b51c3fd75b25c75f068099`,
  last modified 2026-11-13 13:07 GMT, one day after the 2026-11-12 receipt
  that counted three. It is not applied. It changes `qSspiContinue()` in
  `src/network/kernel/qauthenticator.cpp`, lines 1718 to 1750, which sit inside
  the `#if QT_CONFIG(sspi)` block (lines 1655 to 1787). The configure cache has
  `FEATURE_sspi:BOOL=OFF`, and SSPI is Windows-only. It applies cleanly
  ([source/patch-applicability.txt](source/patch-applicability.txt)) and has no
  effect on this build. Every other CVE row in the listing names a module this
  build does not contain (qt3d, qt5compat, qtconnectivity, qthttpserver,
  qtwebsockets).

## Source preparation and configure (container)

| Check                                                               | Result                                                                                                                                                                                                                                                             | File                                                                                                |
| ------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------- |
| `extract-source.sh`, then both patch scripts, in an empty root      | Archive hash checked, three CVE diffs and two QNX patches applied, exit 0                                                                                                                                                                                          | [prepare-source.log](source/prepare-source.log)                                                     |
| Fresh tree against the tree built on 2026-11-12 and 13              | `diff -rq` exit 0 for qtbase (9796 files), qtdeclarative (8210) and qtshadertools (227)                                                                                                                                                                            | [source-tree-compare.txt](source/source-tree-compare.txt)                                           |
| `configure-qnx.sh` up to the build step                             | Exit 0 in a fresh build directory. The script's own check passed: `CMAKE_C_FLAGS_RELEASE` and `CMAKE_CXX_FLAGS_RELEASE` are `-DNDEBUG   -DNDEBUG -O2`. Of 3132 compile `FLAGS` lines in `build.ninja`, 2485 have `-O2`, 647 have `-O3` and none lack an `-O` level | [configure-only-checks.txt](source/configure-only-checks.txt), [log](source/configure-qnx-only.log) |
| `ninja -n` for the script's target list                             | 1860 steps, nothing compiled                                                                                                                                                                                                                                       | same                                                                                                |
| `FEATURE_*` cache values against the 2026-11-12 `-O2` build's cache | 468 shared values equal. Six added, none changed or removed: `jasper`, `mng`, `system_tiff`, `system_webp`, `tiff`, `webp`, from the qtimageformats tree the current `extract-source.sh` also extracts, which this configure does not build                        | [fresh.features](source/fresh.features), [ref-o2.features](source/ref-o2.features)                  |

**Defect fixed in `extract-source.sh`.** Its `--wildcards "$top/*.*"` pattern let
GNU tar's `*` match `/`, so it also extracted every dotted file in every other
submodule (qtsvg, qtmultimedia and the rest, about 20 partial directories). The
top-level configure then tried to configure qtsvg and stopped with
`Failed to find the host tool "Qt6::svgtoqml"`; the first attempt here failed
that way. The script now passes `--no-wildcards-match-slash`. After the fix the
extracted tree is 271 MB and has only the four requested modules.

What this does not show: that the Qt core build compiles and links from this
fresh tree. The container still holds the 2026-11-12 `qnx-build` (1475 objects,
the libraries and plugins; Core is SHA-256 `38dca9e6…`, equal to `runtime-o2`),
and a full rebuild writes several GB. Free space on the internal disk was 12 GB
against a 10 GB floor, and the Colima volume lives on that disk, so the rebuild
was not started. The 2026-11-12 receipt's `EXIT=0` log lines remain the only
record of a complete build.

**qtimageformats build (container, today).** `build-imageformats-qnx.sh` ran
from the fresh tree against the existing `-O2` qtbase: 201 of 201 steps, exit 0
at 2026-11-21T21:45:34Z, producing `libqwebp`, `libqtiff`, `libqtga`, `libqicns`
and `libqwbmp` ([runtime-current/imageformats-build.log](runtime-current/imageformats-build.log)).
Its configure checks the `-O2` flags and would have stopped otherwise.

## Closure check (host)

New: [`scripts/qt-public/verify-closure.py`](../../../scripts/qt-public/verify-closure.py).
It parses ELF dynamic sections directly, so it needs no SDK. Checks: `sums`,
`needed`, `qt-closed`, `rpath`, `no-sdp`, `notices` (documented in the script).

| Tree                                                                                                                           | sums             | needed             | qt-closed | rpath                        | no-sdp                          | notices                  |
| ------------------------------------------------------------------------------------------------------------------------------ | ---------------- | ------------------ | --------- | ---------------------------- | ------------------------------- | ------------------------ |
| `runtime-o2` (159 files, tarball `452bb9fb…`) [verify](closure/o2-runtime-verify.txt), [json](closure/o2-runtime-closure.json) | pass, 159 of 159 | pass, 16 ELF files | pass      | **fail, 16 of 16 ELF files** | pass                            | pass                     |
| Tree deployed to the QNX VM (36 files) [verify](closure/vm-deploy-verify.txt), [json](closure/vm-deploy-closure.json)          | pass             | pass, 23 ELF files | pass      | **fail, 21 files**           | **fail**: `lib/libeventfd.so.1` | **fail**: no `licenses/` |

Both rows ran before the RPATH rewrite. The staged current runtime, after it, is in
the next section.

The 35 file hashes in `docs/evidence/qnx-vm-2026-11-20/vm/deploy.sha256` for
`qt/lib`, `qt/plugins` and `qt/qml` all equal the files pulled from the VM. The
VM tree is a test deployment and was never meant to be a package; its failures
show what a package built from it would still lack.

The `-O2` runtime's system-library list is unchanged from the 2026-11-12
receipt: `libEGL.so.1 libGLESv2.so.1 libc++.so.2 libc.so.6 libeventfd.so.1
libfsnotify.so.1 libgcc_s.so.1 libm.so.3 libscreen.so.1 libslog2.so.1
libsocket.so.4 libzstd.so.1`.

## Current closure, staged (container, host)

[runtime-current/](runtime-current/) holds the records; the files themselves
(55 MB) are not committed, as in the earlier receipts.

1. `spdx-audit.py` on the existing QNX qtbase/qtdeclarative build for the 19
   targets of `configure-qnx.sh` (the README's list plus `QTlsBackendOpenSSLPlugin`,
   `QJpegPlugin`, `QGifPlugin`): 1455 objects, 7044 files read, 13 Qt-tree files
   with no LGPL option in their first 40 lines (8 `Unicode-3.0`, 1 `CC0-1.0`, 1
   `FTL OR GPL-2.0-only`, 1 `BSD-2-Clause AND Imlib2`, 2 with no expression) and
   21 bundled components, one more than the 2026-11-12 audit: libjpeg
   (`IJG AND BSD-3-Clause`) is new because the JPEG plugin is now audited
   ([audit-base.log](runtime-current/audit-base.log)). qtimageformats: 176
   objects, 1510 files, 0 without an LGPL option, libwebp (BSD-3-Clause) and libtiff
   bundled. qtsvg: 27 objects, 1547 files, 0 without an LGPL option, zlib bundled.
2. `stage-runtime.py` with the three audits, `QT_EVAL_MANIFEST` pointed at a
   nonexistent path so no evaluation file is read: exit 0, 208 entries (22
   symlinks, 147 under `licenses/`), 26 ELF files, `DT_NEEDED` closed
   ([stage-runtime.log](runtime-current/stage-runtime.log),
   [manifest.json](runtime-current/manifest.json),
   [needed.json](runtime-current/needed.json),
   [THIRD_PARTY_ATTRIBUTIONS.txt](runtime-current/THIRD_PARTY_ATTRIBUTIONS.txt)).
3. `normalize-rpath.py --runpath /data/qt612-public/lib`: 26 files changed,
   `manifest.json` updated, then `SHA256SUMS` written
   ([runtime-files.sha256](runtime-current/runtime-files.sha256), 188 runtime files; the runtime itself is not committed, so this file does not resolve here).
4. `verify-closure.py --runpath-prefix /data/qt612-public/`: **all seven checks
   pass** ([verify.txt](runtime-current/verify.txt), [closure.json](runtime-current/closure.json)):
   `sums` 188 of 188, `manifest`, `needed` (26 ELF files, the same 12 QNX system
   libraries), `qt-closed`, `rpath`, `no-sdp`, `notices`.
5. Tarball: `tar --no-xattrs --no-mac-metadata -czf runtime-current.tar.gz`,
   SHA-256 `50852d2111fdc0f961f4dc03466aea12b511ecccf46e96ed04337ece2c45531c`,
   19,843,158 bytes ([tarball.txt](runtime-current/tarball.txt)).

**What the stage is made of.** The Qt libraries, the QNX plugin, the TLS, JPEG
and GIF plugins and the QML plugins are the 2026-11-12/13 `-O2` build products
(same hashes as `runtime-o2` before the RPATH rewrite). `libQt6Svg` and
`libqsvg` come from the earlier `svg-qnx` build. The five image-format plugins
were built today. So this is a staging of existing and new products under the
current recipe, not a clean build. The QNX plugin does not contain
`qtbase-qnx-software-grab.patch`, which `apply-qnx-patches.sh` does not apply
(README of `scripts/qt-public`); it applies cleanly to the fresh tree
([patch-applicability.txt](source/patch-applicability.txt)).

**QNX VM load check.** The runtime was unpacked to `/data/qt612-public` on the
VM (its `DT_RUNPATH` target). A short `dlopen(RTLD_NOW)` probe
([vm/dlo.c](vm/dlo.c), built with the SDK `qcc`) ran once per ELF file with
`LD_LIBRARY_PATH` holding only `libeventfd.so.1` and `libzstd.so.1` (the VM
image lacks both; copied from the VM's earlier deploy, not part of the
runtime) and the QNX Screen libraries, never the Qt directory:
`files=26 failed=0` ([vm/dlo-all.txt](vm/dlo-all.txt)). A first attempt with the
VM's `ldd` printed `Could not clean up dynamic TLS memory` for every file, a loader
teardown message and not an unresolved dependency, and the first `dlopen` run
failed on the missing `libzstd.so.1`; both are why the probe and the extra
library exist.

**App run.** The gallery app (`org.rnqnx.showcase`, 2026-11-20 build, not
relinked; RPATH rewritten to `/data/qt612-public/lib:/data/dist-test2/app`) ran
`--bundle proof-button.hbc --screenshot` against this runtime: exit 0,
`RNQNX_PLAYGROUND_SCREENSHOT … 1280x800`, 11 Qt libraries loaded from
`/data/qt612-public/lib` through the runpath, the screenshot byte-identical to
the earlier runs (`b4e67b3c…`) ([vm/cur-run1.log](vm/cur-run1.log),
[png](vm/cur-run1.png), [run-dist2.sh](vm/run-dist2.sh),
[hashes](vm/current-hashes.txt)). The QNX plugin for this run came from the
VM's earlier deploy (`libqqnx.so`, the software-grab build, rewritten the same
way), because the runtime's own plugin aborts without an EGL display:
`QQnxiIntegration: failed to obtain EGL display: 3008` on the VM, which has no
GPU. That first run is the reason for the substitution; it is not repeated in
the evidence folder. So the app run shows the staged libraries work with the app, and not
that the staged plugin renders.

## RPATH defect and fix

**Finding (host, QNX VM).** Every ELF file in the staged `-O2` runtime carries a
build-host `DT_RPATH`: `/home/qnxbuild/qt-6.12.0-public/qnx-build/qtbase/lib`.
`libQt6Core` carries a string of 22 colons, which is 23 empty entries, meaning
the current directory. The RN executable carries three build-host directories and
a trailing empty entry. [packaging](../../packaging.md) already required that
shipped binaries not carry one, because QNX's loader searches `DT_RPATH` before
`LD_LIBRARY_PATH`, so a replacement library would lose to a directory the user
may have. That text recommended a relative `$ORIGIN` path.

**Probe on the QNX VM** ([vm/rpath-probe.txt](vm/rpath-probe.txt); sources and
build in `vm/foo1.c`, `foo2.c`, `main.c`, `build.sh`): a program linked to
`libfoo.so.1`, version 1 in the build-host directory, version 2 in a
`LD_LIBRARY_PATH` directory.

| Case | Binary                                                             | Library placement                                | Loaded                                                                |
| ---- | ------------------------------------------------------------------ | ------------------------------------------------ | --------------------------------------------------------------------- |
| A    | `DT_RPATH` `/data/dist-probe/buildhost`                            | v2 on `LD_LIBRARY_PATH`                          | **v1**, from build-host dir: replacement ignored                      |
| O    | `DT_RPATH` `$ORIGIN/lib`                                           | v1 in `./lib`                                    | **not found**: the loader tried the literal `$ORIGIN/lib/libfoo.so.1` |
| B    | after `normalize-rpath.py`: `DT_RUNPATH` `/data/dist-probe/rp/lib` | v1 in that directory, no `LD_LIBRARY_PATH`       | v1 from the runpath directory                                         |
| C    | same as B                                                          | v2 on `LD_LIBRARY_PATH`                          | **v2**                                                                |
| D    | same as B                                                          | v2 copied over the file in the runpath directory | **v2**                                                                |

So on QNX 8.0.5 (`uname` in the probe) the build-host `DT_RPATH` defeats library
replacement, `$ORIGIN` does not work, and an absolute `DT_RUNPATH` gives the
order the LGPL replacement needs: `LD_LIBRARY_PATH` first, then the install
directory.

**Fix.** New: [`scripts/qt-public/normalize-rpath.py`](../../../scripts/qt-public/normalize-rpath.py)
rewrites each `DT_RPATH` (and a `DT_RUNPATH` with an absolute or empty entry)
to a `DT_RUNPATH` with a given absolute path, in place in `.dynstr`, so no
offset or size changes; it refuses a string that does not fit and is idempotent.
It is a post-link step for the staged files, not a build fix. The build still
writes the RPATH.

**Check of the tool (container, QNX SDP).** On the VM-deployed tree plus the app
binary (24 ELF files read by `ntoaarch64-readelf -d`): 0 with `DT_RPATH`, 22
with `DT_RUNPATH`, all `/data/dist-test/qt/lib` (the app has
`/data/dist-test/qt/lib:/data/dist-test/app`)
([vm/sdk-readelf-after-normalize.txt](vm/sdk-readelf-after-normalize.txt)).
`verify-closure.py` on the normalized Qt tree: `rpath` passes
([vm/normalized-verify.txt](vm/normalized-verify.txt); `notices` fails because
that tree has no `licenses/`, and `libeventfd.so.1` is excluded).

## Library replacement on the QNX VM

The gallery app `org.rnqnx.showcase` (the 2026-11-20 deploy, not rebuilt, plus
its VM-deployed Qt tree) was copied to `/data/dist-test`, both normalized as
above, and run with `--bundle proof-button.hbc --screenshot`, `DL_DEBUG=libs`,
and a `LD_LIBRARY_PATH` that held only the QNX Screen libraries and the two QNX
system libraries the VM image lacks (`libeventfd.so.1`, `libzstd.so.1`), never
the Qt directory. [vm/run-dist.sh](vm/run-dist.sh) is the script. Hashes are in
[vm/hashes.txt](vm/hashes.txt).

| Run         | Setup                                                                                                                                                                                            | Result                                                                                                                                                                                                                                                                        |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `norm-run1` | Normalized app and Qt tree, no extra path                                                                                                                                                        | Exit 0. Eleven Qt libraries came from `/data/dist-test/qt/lib` through the runpath, `libqqnx.so` and the Shapes QML plugin from the tree, `libhermesvm.so` from the app directory. `RNQNX_PLAYGROUND_SCREENSHOT … 1280x800`. [log](vm/norm-run1.log), [png](vm/norm-run1.png) |
| `repl-run1` | Same, plus `/data/dist-test/replace` first on `LD_LIBRARY_PATH` holding `libQt6Core.so.6` from `runtime-o2` (SHA-256 `38dca9e6…`, a different build of the same source; the VM's is `8bdc33d8…`) | Exit 0. `libQt6Core.so.6` loaded from `/data/dist-test/replace/`, every other Qt library still from the runpath directory. Screenshot byte-identical to `norm-run1` (SHA-256 `b4e67b3c…`). [log](vm/repl-run1.log), [png](vm/repl-run1.png)                                   |

This is the LGPL-3.0 section 4 replacement path for a dynamically linked Qt,
shown on the QNX VM with the gallery app. It used a replacement built from the
same source and the same patches except `software-grab`; it does not show a
replacement built by someone else from the source offer, because that offer does
not exist yet.

Limits of these runs: software rendering under TCG, one card, one frame; no
input, no GPU, no timing. They say nothing about the Pi 400 or a Pi 4.

## What is still missing, exactly

1. **A clean end-to-end build** of host tools and the QNX target with the
   current recipe (`configure-host.sh`, `configure-qnx.sh`, `build-svg-qnx.sh`,
   `build-imageformats-qnx.sh`, `stage-runtime.py`) from the fresh tree, so
   that every file in the runtime comes from one pass. Blocked by the disk floor
   on this machine; the space it needs inside the container's volume was not
   measured.
2. **The software-grab patch decision.** The VM and any no-GPU target need
   `qtbase-qnx-software-grab.patch`; `apply-qnx-patches.sh` does not apply it, so
   the staged QNX plugin differs from the one the VM captures use. Either the
   script applies it (a runtime change that needs its own Pi 400 run, which
   this task may not do) or the package documents two plugins.
3. **A release package and manifest**: the runtime tarball, a source bundle or
   written offer (archive URL and SHA-256, the four-diff decision above, the
   patches, `scripts/qt-public/`), the FreeType credit line and the LGPL
   statement in the product documentation, and release-unit definitions for
   `@rn-qnx` (still provisional; no npm publishing here).
4. **A fresh application link** against the public Qt build. The RN executables
   still link against the earlier libraries; the 2026-11-12 receipt lists this
   too.
5. **The build-level RPATH fix** (stop the build from writing it), so the
   post-link rewrite is not the only guard. Needs the rebuild in item 1.
6. **An independent reproduction** of items 1 to 3 by a lane that did not write
   them (PROMPT-7 verifier lane).

The gate stays `pending` with `evidence: []` in
`docs/releases/v1-readiness.json`. A file's existence does not close a gate and
this receipt names open items against every row of the table at the top.

## Verified and not verified

| Verified here                                                                                                       | Not verified here                                                        |
| ------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| Archive and the three applied diffs match upstream today (host)                                                     | That the Qt core libraries rebuild from the fresh tree                   |
| Fresh source tree equals the tree built on 2026-11-12 and 13 (container)                                            | That every file in the staged runtime comes from one build pass          |
| Configure succeeds at `-O2`, 0 compile lines without an `-O` level (container)                                      | Byte-reproducible binaries (build paths are embedded; not attempted)     |
| qtimageformats plugins build, 201 of 201 (container)                                                                | Any Pi 400, Pi 4 QSTI or release-board behavior                          |
| Staged current runtime: 188 of 188 hashes, all seven closure checks (host)                                          | That the staged `libqqnx.so` renders (it aborts on the VM without a GPU) |
| Build-host `DT_RPATH` defeats replacement, `$ORIGIN` is unsupported, `DT_RUNPATH` works (QNX VM)                    | A written source offer, a source bundle or any release package           |
| 26 of 26 ELF files load on the VM through the runpath; gallery app runs, also with a replaced `libQt6Core` (QNX VM) | A fresh application link against these libraries                         |

## Files

Raw logs sit beside this file and are listed in [SHA256SUMS.txt](SHA256SUMS.txt).
The staged runtime directory and tarball (55 MB, 19.8 MB) are not committed.
Scratch trees in the build container and the VM test directories
(`/data/dist-test`, `/data/dist-test2`, `/data/dist-probe`, `/data/qt612-public`,
`/data/dist-sys`) were deleted after the runs.
