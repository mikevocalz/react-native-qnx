# Distribution gate: one-pass clean-tree build, 2026-11-22 and 2026-11-23

**The `distribution` gate stays `pending` with `evidence: []`.** This receipt
closes one open item from [the 2026-11-22 receipt](../distribution-2026-11-22/README.md#state-of-the-open-items):
a single run of the final scripts from an empty tree, with no reuse of earlier
source or host tools. It ran once to the end and produced a staged runtime that
passes the closure checks. Four other items stay open and are listed at the end.
Nothing here is Pi 400 or release-board evidence.

Labels: **host** is macOS on the Mac. **container** is `qt-onepass-c`, a new
Linux x86_64 container (Ubuntu 24.04, QNX SDP 8.0.5 `qcc` 12.2.0, 6 CPUs,
11.7 GB memory). **QNX VM** is QEMU, TCG, software render, not a GPU. Timings in
the VM rows are not performance evidence. Repository base: `ae0040d3` (main after
PR #217). Hashes of every file in this folder are in
[SHA256SUMS.txt](SHA256SUMS.txt); `shasum -a 256 -c SHA256SUMS.txt` run in this
folder checks them.

## Environment

[build/container-facts.txt](build/container-facts.txt),
[build/apt-packages.txt](build/apt-packages.txt) (`apt-mark showmanual`, which
includes the base image's own packages)

- **Container.** `ubuntu:24.04` pulled with `--platform linux/amd64`, started with
  `--privileged` (for the loop mount). It was not committed to an image. The SDP
  is bound read-only at `/work` and the T9 at `/t9`. Packages came from the #217
  list with `--no-install-recommends`; CMake 3.31.9 is the Kitware tarball.
  `qnx805-build` and image `fe81560990fa` were not modified.
- **Work directory.** Sources and build trees live in a 120 GB sparse ext4 image
  on the T9, loop-mounted at `/build`, because the T9's virtiofs mount refuses
  `open(O_CREAT, 0200)` (see the #217 receipt). The archive and CVE diffs were
  copied into `/build/root/download` from the host's download directory before the
  run; `extract-source.sh` and `apply-cve-patches.sh` checked their hashes.
- **QNX licence supply.** The SDP compilers refuse to run without an activated
  licence (`q++` prints "You don't have a valid license for this product"). The
  licence was supplied from the existing `qnx805-build` container by copying its
  per-user licence, `qconfig` and `qnx_env.ini` items into a directory on the T9
  that served as the container's `HOME`, with the owner's approval. The contents
  were never printed, logged, hashed or committed. One layout detail: the licence
  files must sit directly in `license/`; a copy that left them one level deeper
  (`license/license/`) failed the same check. A one-file compile
  (`q++ -Vgcc_ntoaarch64le t.cpp -o t`) passed before the build started, and
  [docs/packaging.md](../../packaging.md#supplying-the-qnx-licence-to-a-clean-build-container)
  now documents the step. The #217 receipt did not record how its container got
  the licence, so the step is new here.

## Single pass from an empty tree (container)

[build/stages-single-pass.tsv](build/stages-single-pass.tsv), logs
`build/single-pass-*.log`. `scripts/qt-public/single-pass-build.sh` ran
`extract-source.sh`, `apply-cve-patches.sh`, `apply-qnx-patches.sh`,
`configure-host.sh`, `configure-qnx.sh`, `build-svg-qnx.sh` and
`build-imageformats-qnx.sh` once, with `JOBS=4`, in a freshly created ext4 image.
Every stage exited 0.

| Stage                     | Wall time               | Steps         | Exit |
| ------------------------- | ----------------------- | ------------- | ---- |
| extract                   | 22 s                    | 4 submodules  | 0    |
| CVE patches, QNX patches  | 1 s, 0 s                | 3 and 2 diffs | 0    |
| host tools                | 2308 s                  | 2130 of 2130  | 0    |
| Qt core for QNX aarch64le | 1991 s                  | 1860 of 1860  | 0    |
| qtsvg                     | 65 s                    | 42 of 42      | 0    |
| qtimageformats            | 59 s                    | 201 of 201    | 0    |
| **Total**                 | **4446 s (74 min 6 s)** |               |      |

Started 2026-11-22T23:36:58Z, finished 2026-11-23T00:51:04Z. The host-tools
stage took 2308 s here and 2126 s and 2841 s in two earlier runs of the same
script; the spread is the host's load and was not investigated.

- **Peak disk use of the work directory** (`du -sk /build`, one row a minute in
  [build/disk-samples-single-pass.tsv](build/disk-samples-single-pass.tsv)):
  1,580,004 KiB (1.51 GiB) at the end of the imageformats stage. The 2026-11-22
  receipt reported 1.09 GiB for pass 1. I did not find out where the extra 0.4 GiB
  sits.
- **Earlier starts that did not count.** Two earlier starts of this run were
  discarded and are not in this folder: the first stopped at the start of the Qt
  core stage because the container had no licence (host tools had finished), and
  the second was stopped by my free-space guard at step 296 of 2130 of host
  tools. This receipt describes the third start only, which ran from an empty
  tree to the end.

## Closure, RPATH and normalisation (container)

[runtime/](runtime/)

- `audit-and-stage.sh` ran the three SPDX audits (base 1455 objects and 13 Qt
  files without an LGPL option, imageformats 176 and 0, qtsvg 27 and 0) and
  `stage-runtime.py`: 208 entries, 26 ELF files, 188 hashed files,
  `DT_NEEDED` closed, 12 QNX system libraries named.
- `verify-closure.py --runpath-prefix /data/qt612-public/` printed `RESULT PASS`
  for all seven checks ([runtime/verify.txt](runtime/verify.txt),
  [runtime/closure.json](runtime/closure.json)): `sums` (188 listed, 187 files; one
  entry is a symlink), `manifest`, `needed` (26 ELF files, 12 system libraries),
  `qt-closed`, `rpath`, `no-sdp`, `notices`. `sums` read
  [runtime/runtime-files.sha256](runtime/runtime-files.sha256), which I wrote with
  `sha256sum` after staging because the script does not write it.
- `normalize-rpath.py --check` printed `0 file(s) would change`
  ([runtime/normalize-check.txt](runtime/normalize-check.txt)).
- `readelf -d` over the 26 staged ELF files
  ([runtime/readelf-counts.txt](runtime/readelf-counts.txt)): **0 `DT_RPATH`, 26
  `DT_RUNPATH`, all 26 exactly `/data/qt612-public/lib`.** The linker-output dump
  taken before staging ([build/single-pass-dynsec.txt](build/single-pass-dynsec.txt))
  shows the same.
- Tarball `runtime-single-pass.tar.gz`, 19,857,940 bytes, SHA-256
  `2b131ccf…` ([runtime/tarball.txt](runtime/tarball.txt)). It is not committed.

**Comparison with the 2026-11-22 runtime.** Of 188 file hashes, 181 are
identical to [the earlier list](../distribution-2026-11-22/runtime/runtime-files.sha256).
Seven differ: six libraries (`libQt6Qml`, `QmlMeta`, `QmlModels`,
`QmlWorkerScript`, `Quick`, `QuickShapes`, each `.so.6.12.0`) and
`manifest.json`, which records the hashes. I did not look into why those six
differ. The two builds used the same source archive, patches and scripts, and the
other 20 ELF files, including `libQt6Core`, `Gui`, `Network`, `OpenGL`, `Svg`
and `libqqnx.so`, match byte for byte.

## App link (container)

[app/](app/). `scripts/qt-public/build-app-qnx.sh` built `rnqnx_playground_qnx`
(Release, `-O2`) in an empty build directory against this Qt. The first attempt
(`JOBS=4`) was stopped by my free-space guard at step 390 of 419 after 973 s (see
the disk section) and left no usable output. The second ran with `JOBS=2` in a new
directory: 419 of 419 steps, **1466 s**, exit 0
([app/app-build.log](app/app-build.log)). The binary, SHA-256 `e1c6f6e7…`, needs
`libQt6{Core,Gui,Network,OpenGL,Qml,Quick}.so.6`, `libhermesvmlean.so` and system
libraries, and carries **`DT_RUNPATH` `/data/qt612-public/lib` and no `DT_RPATH`**
([app/app-dynsec.txt](app/app-dynsec.txt), [app/app-hashes.txt](app/app-hashes.txt)).
It differs from the 2026-11-22 binary (`534378c9…`) and the repository base moved
from `18e708d8` to `ae0040d3`; I did not diff the two. Hermes is the existing QNX
build (`libhermesvmlean.so`, `c3e02a14…`, same hash as before). The React Native
third-party sources are the 2026-11 consumer build's fetched copies. The
bundle `playground.hbc` (1,141,001 bytes, SHA-256 `aad9e61a…`) came from Metro
(`--platform qnx --dev false`) and the host `hermesc-260318099.0.5-linux`.

## Run on the QNX VM (QEMU, TCG, software render; not board evidence)

[vm/](vm/). The runtime tarball was unpacked to `/data/qt612-public`; all 188
files hash equal to `runtime-files.sha256` (checked on the VM with `openssl dgst`,
188 ok, 0 bad). The app ran `--fullscreen --bundle playground.hbc --screenshot`
with `LD_LIBRARY_PATH` holding only the QNX Screen libraries, the two VM system
libraries (`libeventfd.so.1`, `libzstd.so.1`, from the VM's earlier deploy) and
the Hermes library, never the Qt directory ([vm/run.sh](vm/run.sh)).

| Run                                                    | Plugin                                                                | Result                                                                                                                                                          |
| ------------------------------------------------------ | --------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [own-plugin](vm/own-plugin.log)                        | the runtime's own `libqqnx.so` (`4a560e39…`, same hash as 2026-11-22) | `QQnxIntegration: failed to obtain EGL display: 3008`, abort, exit 134. The VM has no GPU.                                                                      |
| [sw-plugin](vm/sw-plugin.log), [png](vm/sw-plugin.png) | the VM lane's software-grab `libqqnx.so` (`531bdd53…`)                | exit 0, `RNQNX_PLAYGROUND_SCREENSHOT … 1280x800`, PNG 24,464 bytes, SHA-256 `787bde3b…`, the same as on 2026-11-22. Libraries loaded from `/data/qt612-public`. |
| [repl](vm/repl.log)                                    | same plugin; a copy of `libQt6Svg.so.6` first on `LD_LIBRARY_PATH`    | exit 0; `DL_DEBUG` shows `libQt6Svg.so.6 loaded from: /data/dist3/repl/libQt6Svg.so.6`. A replacement library wins.                                             |

In the `own-plugin` log the loader prints `runpath=/data/qt612-public/lib` for the
Qt libraries (22 lines).

**Software-grab patch, not decided here.** The staged runtime's own plugin does
not render on the VM: it aborts at the EGL display, exactly as before. The
rendering run used the earlier lane's plugin, which comes from
`qtbase-qnx-software-grab.patch` and is not in `apply-qnx-patches.sh`. Whether
that patch should be applied is the owner's decision
([proposals/software-grab-decision.md](../distribution-2026-11-21/proposals/software-grab-decision.md)).
The run shows that the freshly built libraries and a freshly linked app load and
render together, and not that the runtime's own plugin renders.

**Pi 400.** No step in this receipt ran on the Pi 400, and nothing was launched
there.

## Host disk, swap and the guard

My builds write only to the T9. The host's internal disk free space (`df -k /`)
still swung by up to 2.3 GiB within 30 seconds while they ran. A guard sampled it
every 30 to 60 s and killed ninja below a threshold: 10.5 GiB at first, 8 GiB
after the coordinator relaxed it.

- Samples: [phase 1](build/internal-free-phase1.tsv) (single pass, 2026-11-22T23:33Z
  to 2026-11-23T00:51Z), [phase 2](build/internal-free-phase2.tsv) and
  [phase 3](build/internal-free-phase3.tsv). **There is no sample between 00:51:09Z
  and 01:02:53Z**, because the first guard exited when the single pass finished
  and I restarted it late; the audit, staging and closure checks ran in that gap.
- During the single pass the lowest sample was 12,728,620 KiB (12.14 GiB) at
  2026-11-23T00:00:06Z. The highest was 13,925,132 KiB. The hard stop never fired
  during the single pass.
- The guard fired at 2026-11-23T01:08:54Z with 9,803,816 KiB (9.35 GiB) free
  during the first app attempt, then at 01:09:32Z (9,806,152) and 01:10:04Z
  (10,854,756) as free space recovered ([build/guard-trip.txt](build/guard-trip.txt)).
  By 01:10:03Z it was back above 10.5 GiB. The lowest sample of the `JOBS=2`
  attempt was 10,293,188 KiB (9.82 GiB).
- **Likely cause.** `sysctl vm.swapusage` showed 8698 MB of 9216 MB used at about
  01:10Z, and the biggest files on the internal disk were not growing
  (`~/.colima` 27.4 GB, Chrome's code-sign clone 22.6 GB, `~/Library/Caches`
  0.78 GB). So macOS swap is the likely source of the swings. I could not read the
  swap files, so this is an inference.

## State of the open items

| Item                               | After this pass                                                                                                                                                                                                             |
| ---------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1 Clean single-pass rebuild        | **Done once** from an empty tree with the final scripts: 74 min 6 s, 1.51 GiB peak, closure checks pass, 0 `DT_RPATH`, 26 `DT_RUNPATH`. Done in a container with a supplied licence; a second independent run was not made. |
| 4 Fresh app link against public Qt | Done again for the playground app (link, hashes, VM run), as on 2026-11-22. The VM run again needed the substitute plugin.                                                                                                  |
| 5 Build-level RPATH fix            | Verified again on a fresh tree: `normalize-rpath.py --check` reports nothing to change.                                                                                                                                     |

**Still open for the gate** (nothing here closes them): the owner's decision on
the software-grab patch and so a runtime plugin that renders without a GPU; the
owner's review of the source offer; a package manifest with
`releaseEligible: true` produced from this runtime; the written release units
published; and the independent Verifier reproduction, which has not happened.
`docs/releases/v1-readiness.json` is unchanged.

## Cleanup

The clone, the container, the work directory (including the loop image and the
tools), the copied licence home and the VM scratch directories are deleted after
the pull request is pushed. The values are in the pull request.

## Files

`build/` has the stage table, disk and free-space samples, stage logs, the
pre-staging dynamic-section dump and container facts. `runtime/` has the audits,
manifest, closure result, readelf counts and notices of the staged runtime.
`app/` has the app build log and hashes. `vm/` has the VM logs, the screenshot
and the run script. The staged runtime, tarballs and app binary are not committed.
