# Proposal: the software-grab patch and the shipped QNX plugin

**For Mike to decide. Nothing here is decided.** This lane did not change
`apply-qnx-patches.sh` and will not until the owner picks an option.

## The facts

- `scripts/qt-public/patches/qtbase-qnx-software-grab.patch` (SHA-256
  `d19a2e97450a13cea9362c61cdff33326f39e856e4b2df48b51219751c72a815`) touches
  three files under `src/plugins/platforms/qnx/`: 4 hunks, 34 added lines, 3
  removed lines.
  - `QQnxRasterBackingStore::toImage()` returns a copy of the current render
    buffer. Without it `grabWindow()` returns a null image with the software
    scene graph.
  - `QQnxIntegration::createEglDisplay()` warns and continues, instead of
    `qFatal`, when there is no EGL display and `QT_QUICK_BACKEND` is `software`
    or `softwarecontext`. It still calls `qFatal` otherwise.
- `apply-qnx-patches.sh` applies the offscreen-surface and resize-swap patches
  only. So the `libqqnx.so` in the staged runtime does not hold this patch.
- The QNX VM captures (2026-11-20) and the distribution VM run ran a plugin
  with the patch applied by hand. On the VM, the staged runtime's own
  plugin aborts with `QQnxiIntegration: failed to obtain EGL display: 3008`
  ([distribution receipt](../README.md), "App run").
- The patch applies cleanly to a fresh patched qtbase tree
  ([patch-applicability.txt](../source/patch-applicability.txt), host,
  container).
- The patch has never run on the Pi 400, and this lane may not touch it.

So the plugin that every VM screenshot comes from is not the plugin the package
would carry, and no evidence ties the two together except the patch text.

## Options

| Option | What changes                                                                                           | Gains                                                                                                                              | Costs and risks                                                                                                                                                                                                                                                                          |
| ------ | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| A      | `apply-qnx-patches.sh` applies all three patches. One plugin for every target.                         | One `libqqnx.so` to hash, notice and describe. VM evidence describes the shipped plugin. The source offer lists one patch set.     | Changes the runtime that the Pi 400 receipts describe. The `toImage()` override is active for every raster backing store, and the EGL change sits on the startup path of every run. Needs its own Pi 400 run (cold start, OpenGL path, software path) before release. Not runnable here. |
| B      | Leave the script as is. Document two plugins: the release plugin and a "no-GPU capture" plugin.        | No change to the Pi 400 runtime. Nothing to re-run on the board.                                                                   | Two plugin binaries, two sets of hashes, two notices of patches. VM captures stay evidence about a plugin that is not the release one. A reader can mix the two up.                                                                                                                      |
| C      | The script applies the third patch only when asked (`QNX_SOFTWARE_GRAB=1`). Default output is today's. | The release plugin is byte-for-byte the recipe it is today. The VM plugin is reproducible from the same script instead of by hand. | Still two binaries, as in B. The opt-in must appear in the source offer and in the package manifest of any runtime that uses it. A forgotten variable gives a plugin that aborts on the VM.                                                                                              |

## Recommendation

C now, A after one Pi 400 run.

C removes the by-hand step, so a third party can rebuild the plugin the VM
captures came from, and leaves the Pi 400 receipts untouched. Moving to A needs
the Pi 400 owner to run the three paths named in the table. If that run shows no
difference, A is the smaller long-term package. If the owner prefers to avoid
any change that touches the board runtime, B is the safe choice and costs
only the extra documentation.

## What this proposal does not claim

It makes no claim that the patch is correct on a Pi 400 or on a board with a
GPU, only that it applies and that the VM runs behave as described in the
[VM receipt](../../qnx-vm-2026-11-20/README.md). It does not rank the options
by anything this lane measured.

## Decision record

Owner choice: ____ (A, B or C) Date: ____

Whichever option is chosen, update `scripts/qt-public/README.md`, the source
offer ([docs/distribution/source-offer.md](../../../distribution/source-offer.md))
and `release-units.json` in the same change.
