# Distribution gate: clean rebuild on the T9, build-level RPATH fix, app link, 2026-11-22

**The `distribution` gate stays `pending` with `evidence: []`.** This pass did
open items 1, 4 and 5 of the [second pass](../distribution-2026-11-21/README.md#second-pass-later-on-2026-11-21-what-ran-and-what-did-not):
host tools and the Qt core libraries were rebuilt from the public archive
entirely on the T9, the build now writes `DT_RUNPATH` itself, and a freshly
linked app ran on the QNX VM against the result. What is missing is listed at
the end. Nothing here needed a device, and nothing here is Pi 400 or release
board evidence.

Labels used below: **host** is macOS on the Mac. **container** is
`qt-dist3-build`, a new Linux x86_64 container (Ubuntu 24.04, QNX SDP 8.0.5
`qcc` 12.2.0). **QNX VM** is QEMU, TCG, software render. Timings in the VM rows
are not performance evidence. Repository base: `18e708d8` (main after PR #216).
Hashes of every file in this folder are in [SHA256SUMS.txt](SHA256SUMS.txt).

## Environment

[build/container-facts.txt](build/container-facts.txt),
[build/apt-packages.txt](build/apt-packages.txt)

- **Architecture, measured.** `qnx805-build` reports `uname -m` = `x86_64` (it
  runs under Rosetta). The earlier receipt and `scripts/qt-public/README.md`
  called it aarch64; both are corrected. The SDP's Linux host tools are x86_64
  only.
- **New container, old one untouched.** `ubuntu:24.04` with
  `--platform linux/amd64` and `--privileged` (for the loop mount below), the
  SDP bound read-only at `/work` (the build wrote nothing to it), the T9 at
  `/t9`. Packages are the manual installs `apt-mark showmanual` lists in
  `qnx805-build` minus the ones the base image already has; CMake 3.31.9 is the
  tarball that container holds. `qnx805-build` and image `fe81560990fa` were not
  modified. The Colima configuration was not touched.
- **The T9 refuses a file mode that Qt's build uses.** Colima's virtiofs mount
  fails `open(O_CREAT, 0200)` and `open(O_CREAT, 0000)` with `EACCES`
  ([build/virtiofs-mode-probe.txt](build/virtiofs-mode-probe.txt)).
  libstdc++'s `std::filesystem::copy` creates its destination with the owner-read
  bit off, and Qt's `syncqt` stopped in the first host-tools configure with
  `Unable to remove file ... error: (13)Permission denied`, 418 times. The
  sources and build trees therefore live in a 120 GB sparse ext4 image file at
  `/Volumes/QNX/clones/distribution3-build/work.ext4`, loop-mounted at `/build`
  ([build/mount-work.sh](build/mount-work.sh)); the same probe on `/build` passes.
  Every byte is on the T9. The archive, CMake, the Hermes build tree and the
  staged outputs sit on the T9 directly. The log of that first failed attempt was
  deleted by a cleanup of mine and is not in this folder; the probe reproduces
  its cause.

## Item 1: clean single-pass rebuild (container)

[build/stages-pass1.tsv](build/stages-pass1.tsv), logs `build/pass1-*.log`.
`scripts/qt-public/single-pass-build.sh` ran `extract-source.sh`,
`apply-cve-patches.sh`, `apply-qnx-patches.sh`, `configure-host.sh` (host tools),
`configure-qnx.sh` (qtbase, qtshadertools, qtdeclarative), `build-svg-qnx.sh` and
`build-imageformats-qnx.sh` in that order, once, in an empty tree, with `JOBS=4`
on 6 CPUs and 11.7 GB of memory. The archive hash was checked by the script
(`98ff4f44…` equal to the committed value) and the three CVE diffs and two QNX
patches applied.

| Stage                          | Wall time | Steps / result                           | Exit |
| ------------------------------ | --------- | ---------------------------------------- | ---- |
| extract                        | 38 s      | 4 submodules                             | 0    |
| CVE patches, QNX patches       | 1 s, 0 s  | 3 and 2 diffs                            | 0    |
| host tools                     | 2126 s    | 2130 of 2130 steps (configure ~4 min)    | 0    |
| Qt core for QNX aarch64le      | 1945 s    | 1860 of 1860 steps                       | 0    |
| qtsvg                          | 78 s      | 42 of 42                                 | 0    |
| qtimageformats                 | 60 s      | 201 of 201                               | 0    |
| **Total**                      | **4248 s (70 min 48 s)** |                           |      |

- **Peak disk use.** The work directory (`du -sk /build`, sampled each minute
  in [build/disk-samples-pass1.tsv](build/disk-samples-pass1.tsv)) peaked at
  1,145,320 KiB (1.09 GiB) at the end of the imageformats stage. The ext4 image
  had 1.23 GiB allocated on the T9 at that point. Held beside it: the 423 MiB
  archive and 173 MiB CMake (not counted in `du`). Trees at the end, in KiB:
  source 284,280, `host-build` 499,788, `qnx-build` 349,228, `svg-qnx` 4,332,
  `imageformats-qnx` 7,096. The 2026-11-21 estimate of "several GB" was too high.
- **Optimization level.** The configure check passed (`CMAKE_C_FLAGS_RELEASE` and
  `CMAKE_CXX_FLAGS_RELEASE` end in `-O2`). Of 3132 compile `FLAGS` lines in
  `build.ninja`, 2485 have `-O2`, 647 have `-O3` (QtCore and QtGui, ADR-0017) and
  none lack a level ([build/olevel-pass2.txt](build/olevel-pass2.txt); pass 1
  has the same counts as the 2026-11-21 configure-only check).
- **What this pass had to repair.** (a) The virtiofs mode problem above. (b)
  After pass 1, running `configure-qnx.sh` again stopped with `Failed to find
  the host tool "Qt6::svgtoqml"` because `build-svg-qnx.sh` had extracted
  `qtsvg` beside `qtbase` (the same class of fault as the 2026-11-21 tar
  wildcard). It now extracts to `$QT_PUBLIC_ROOT/src-svg`, and `stage-runtime.py`
  reads the qtsvg notices from there. (c) The first RPATH attempt, below.

## Item 5: build-level RPATH fix (container)

Pass 1 used the old recipe and reproduced the defect.
[build/pass1-dynsec.txt](build/pass1-dynsec.txt) (hash and search path of all 26
libraries and plugins, from the linker output, before staging): 26 of 26 carry a
`DT_RPATH`: 24 of them `/build/build/qnx-build/qtbase/lib:` (a build-host
directory plus an empty entry), `libQt6Svg` the same with `/build/build/svg-qnx/lib`
in front, and `libQt6Core` the 22-colon string of 23 empty entries, as on 2026-11-12. None carries a `DT_RUNPATH`.

The target side (`qnx-core`, `qtsvg`, `qtimageformats`; host tools do not link
into the runtime) was then rebuilt from empty build directories with the fixed
scripts ([build/stages-pass2.tsv](build/stages-pass2.tsv): 1945 s, 76 s, 59 s,
all exit 0).
[build/pass2-dynsec.txt](build/pass2-dynsec.txt): **0 of 26 carry a `DT_RPATH`;
26 of 26 carry `DT_RUNPATH` `/data/qt612-public/lib`**, with no empty entry.

What changed, and why each part:

| Setting                                                      | Where                                                                                       | Effect                                                                                                                                                     |
| ------------------------------------------------------------ | ------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON`                        | `configure-qnx.sh`, `build-svg-qnx.sh`, `build-imageformats-qnx.sh`, `build-app-qnx.sh`      | Links with Qt's install rpath, `/data/qt612-public/lib`, not the build directory. Also removes the empty entries.                                          |
| `-Wl,--enable-new-dtags` in the SHARED, MODULE and EXE flags | `qt612-public-qnx-aarch64le.cmake`, `qnx-app-qt-public.toolchain.cmake`                     | QNX's `ld` (binutils 2.43) then writes `DT_RUNPATH`. A direct `qcc` link test with and without the flag gave RPATH and RUNPATH. `-z runpath` is ignored by this `ld`. |

Tried and rejected: `-DFEATURE_enable_new_dtags=ON` with `-DTEST_enable_new_dtags=ON`
fails the configure (`QT_FEATURE_elf` is OFF on QNX, so Qt refuses to force the
feature). `FEATURE_rpath` off and `QT_DISABLE_RPATH` were not run: reading
`QtRpathHelpers.cmake` shows they only control the `INSTALL_RPATH` Qt adds, not
the build-tree rpath CMake writes, which is the one that reaches the staged files.

**Staged runtime, built from pass 2** ([runtime/](runtime/)):
`audit-and-stage.sh` ran the three SPDX audits (base 1455 objects and 13 Qt
files without an LGPL option, imageformats 176 and 0, qtsvg 27 and 0, the same
counts as 2026-11-21) and `stage-runtime.py` (208 entries, 22 symlinks, 186
hashed files, 147 under `licenses/`, 26 ELF files, `DT_NEEDED` closed, the same
12 QNX system libraries). `normalize-rpath.py --check` printed `0 file(s) would
change` ([normalize-check.txt](runtime/normalize-check.txt)), so no rewrite ran.
`verify-closure.py --runpath-prefix /data/qt612-public/` passed all seven checks
([verify.txt](runtime/verify.txt), [closure.json](runtime/closure.json)): `sums`
188 of 188, `manifest`, `needed`, `qt-closed`, `rpath`, `no-sdp`, `notices`.
Tarball `runtime-single-pass.tar.gz`, 19,868,571 bytes, SHA-256 `c24ebd28…`
([tarball.txt](runtime/tarball.txt)); it is not committed.
`normalize-rpath.py` is now a check; its rewrite stays for older runtimes.

## Item 4: fresh app link and a run on the QNX VM

**Link (container).** `scripts/qt-public/build-app-qnx.sh` configured and built
`rnqnx_playground_qnx` (`-O2`, Release, playground target of the repository's
CMake) in an empty build directory against the pass-2 Qt:
[app/app-build.log](app/app-build.log), 419 of 419 steps, **835 s**, exit 0
(first attempt, [app-build-attempt1-werror.log](app/app-build-attempt1-werror.log),
stopped at step 119 in React Native's own `-Werror` on a GCC 12 false
`maybe-uninitialized` in `ResizeObserverManager.cpp`; the script now demotes the
same `-Wno-error=` list the 2026-11-19 PR 149 `-O2` build used). The binary,
SHA-256 `534378c9…`, needs `libQt6{Core,Gui,Network,OpenGL,Qml,Quick}.so.6`,
`libhermesvmlean.so` and system libraries, and carries **`DT_RUNPATH`
`/data/qt612-public/lib` and no `DT_RPATH`** ([app/app-dynsec.txt](app/app-dynsec.txt),
[app/app-hashes.txt](app/app-hashes.txt)). Hermes is the existing QNX build
(`libhermesvmlean.so`, `c3e02a14…`), not rebuilt here; the React Native
third-party sources come from the 2026-11 consumer build's fetched copies.

**Run (QNX VM, TCG, software render; never board evidence).** The runtime
tarball was unpacked to `/data/qt612-public` on the VM; all 188 files hash equal
to `runtime-files.sha256` ([vm/runtime-hashes-on-vm.txt](vm/runtime-hashes-on-vm.txt),
`sha256sum -c` does not exist on the VM, so `openssl dgst` was used). The app ran
`--fullscreen --bundle playground.hbc --screenshot` with `LD_LIBRARY_PATH` holding
only the QNX Screen libraries, the two VM system libraries (`libeventfd.so.1`,
`libzstd.so.1`, copied from the VM's earlier deploy, not part of the runtime) and
the Hermes library. Never the Qt directory ([vm/run.sh](vm/run.sh)).

| Run                                              | Plugin                                  | Result                                                                                                                                                       |
| ------------------------------------------------ | --------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [own-plugin](vm/own-plugin.log)                  | the runtime's own `libqqnx.so` (`4a560e39…`) | `QQnxiIntegration: failed to obtain EGL display: 3008`, abort, exit 134. The VM has no GPU. Same result as 2026-11-21.                                  |
| [sw-plugin](vm/sw-plugin.log), [png](vm/sw-plugin.png) | the VM lane's software-grab `libqqnx.so` (`531bdd53…`) | exit 0, `RNQNX_PLAYGROUND_SCREENSHOT … 1280x800`, PNG 24,464 bytes (`787bde3b…`) showing the playground's View and Text card. 11 Qt libraries loaded from `/data/qt612-public/lib`. |
| [repl](vm/repl.log)                              | same plugin; a copy of `libQt6Svg.so.6` in a directory first on `LD_LIBRARY_PATH` | exit 0; `DL_DEBUG` shows `libQt6Svg.so.6 loaded from: /data/dist3/repl/libQt6Svg.so.6`. A replacement library wins, which the old build-host `DT_RPATH` prevented. |

In the `own-plugin` log the loader prints `rpath=NULL runpath=/data/qt612-public/lib`
for the app's Qt libraries.

**Caveat, as before.** The staged runtime's own plugin does not render on the VM,
so the rendering run used a plugin that is not in the runtime: the earlier VM
lane's `libqqnx.so`, built from `qtbase-qnx-software-grab.patch`, which still has
a build-host `DT_RPATH` (a directory that does not exist on the VM, so it is
inert here). That patch is not in `apply-qnx-patches.sh`. Whether it should be is
the owner's decision
([proposals/software-grab-decision.md](../distribution-2026-11-21/proposals/software-grab-decision.md));
this pass did not decide it, and the plugin used is hashed in
[vm/substitute-plugin.txt](vm/substitute-plugin.txt). So the run shows that the
freshly built libraries and a freshly linked app load and render together, and
not that the runtime's own plugin renders.

## State of the open items

| Item                                | After this pass                                                                                                                                                                                                      |
| ----------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1 Clean single-pass rebuild         | **Done once, with the old recipe** (70 min 48 s, 1.09 GiB peak). The runtime that was staged comes from the target-side rebuild with the fixed scripts, which reused pass 1's source tree and host tools. A single run of the final scripts from an empty tree is **not done**; it is about 72 minutes by these timings. |
| 4 Fresh app link against public Qt  | **Done** for the playground app: link, hashes, VM run. The gallery/consumer app was not rebuilt; the VM run needed a substitute plugin (above).                                                                       |
| 5 Build-level RPATH fix             | **Done and verified** for the Qt libraries, plugins and the app: 0 `DT_RPATH`, `DT_RUNPATH` `/data/qt612-public/lib` throughout, `normalize-rpath.py --check` reports nothing to change.                              |

**Still open for the gate** (nothing here is closed by this paragraph): the
owner's decision on the software-grab patch and so a runtime plugin that
renders without a GPU; the owner's review of the source offer; a package
manifest with `releaseEligible: true` produced from a runtime built in one run of
the final scripts; the written release units published; and the independent
Verifier reproduction, which has not happened. `docs/releases/v1-readiness.json`
is unchanged.

## Disk and cleanup accounting

- Internal disk free: about 17 GiB at the start of the pass (`df -h` showed
  `17Gi`). It fell to 15.25 GiB after the container's apt layer and to 13.88 GiB
  after `docker commit` of that container (needed to add `--privileged`), and
  stayed between 13.6 and 13.9 GiB through the builds, which wrote only to the T9.
  That setup drop, about 3 GiB, exceeded the 1 GiB stop rule; the cause is Colima
  disk growth from the Docker image layers (1.1 GB container layer, 1.56 GB
  committed image), and the free space stayed above the 10 GB floor. Every build
  and test output of this pass is on the T9.
- pnpm ran with `--store-dir /Volumes/QNX/cache/pnpm` and `npm_config_cache`
  on the T9.
- Removed at the end: the clone, build and store directories, the container and
  its image, the VM scratch directories. The values are in the pull request.

## Files

`build/` has the stage tables, disk samples, logs, dynamic-section dumps,
container facts and the container scripts. `runtime/` has the audits, manifest,
closure result and notices of the staged runtime. `app/` has the app build logs
and hashes. `vm/` has the VM logs, the screenshot and the hashes. The staged
runtime, the tarballs and the app binary are not committed.
