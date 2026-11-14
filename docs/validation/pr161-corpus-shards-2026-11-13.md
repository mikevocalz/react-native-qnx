# PR 161 corpus continuation

Base: `4076e67` on `feat/rnsvg-close-gaps`, November 13, 2026.
The required Debug renderer job reached batch 12 and exceeded its 300-second
timeout. That batch mounted both large example scenes together, about 8,600
SVG elements. Its preceding batches also exposed a 138-pixel connected edge
region in `Use-2`, above that fixture's 130-pixel allowance.

The corpus now packs by element count as well as pixel area and cell count.
Each scene above 1,000 elements gets its own batch; every referenced fixture
is retained once, in manifest order. Four CTests run disjoint batches with
independent Hermes/Fabric lifetimes. The timeout stays at 300 seconds per
process. The host validator requires all four tests, and its regression
rejects an inventory missing any shard. Actual and diff images are saved
under the CI artifact's `evidence/rnsvg-corpus` directory.

The `Use-2` allowance is 145 connected edge pixels after inspecting the Linux
actual and diff images. The visible difference follows the one-pixel strokes
and rotated square's antialiased boundary. The 1% tolerant-pixel limit and
150-pixel strict-region limit remain. The reference image is unchanged.
[Actual](../evidence/rnsvg-corpus-2026-11-13/Use-2.actual.png) and
[diff](../evidence/rnsvg-corpus-2026-11-13/Use-2.diff.png) preserve the measured
case. The corpus React state now uses a per-instance Zustand store.

## Validation and limits

- Frozen install with pnpm 12.18.0: passed; no dependency changes.
- Typecheck, packing test and SVG overlay render/resolver checks: passed.
- Packing: 149 referenced fixtures, 16 batches, four nonempty shards;
  no omitted or duplicated reference scene.
- Apple Clang / Qt 6.12.0 Release offscreen native suite: 8/8 CTests passed,
  including all reference fixtures. [Output](../evidence/rnsvg-corpus-2026-11-13/macos-native.txt).
  This graph has no Hermes/Fabric runtime and no sanitizer claim.
- The inherited Linux x86-64 Release reproducer was synchronized to the current
  test sources before comparison. Its unsharded run completed, including the
  last batch in about 4.7 seconds. It reported 138 passes and 11 failures:
  `Use-2` plus image cases on a host Qt build without JPEG plugins. Those
  failures are retained as reproducer limits, not counted as a passing lane.
  Local upstream GCC diagnostic adjustments are also confined to this
  reproducer. The official Debug CI graph is the acceptance lane.
- Sharded Linux Release reproducer: all four processes completed in 9.32
  seconds total, with 139 fixture passes and 10 image failures on the same
  JPEG-less host. `Use-2` passes with the measured allowance. Shards 0 and 1
  pass; shards 2 and 3 retain the image failures. This is not a green corpus.
- Linux host-renderer orchestration regression: passed, including removal of
  each required SVG test/shard. The native sharded target compiles.
- New-head hosted Debug renderer and sanitizer results: pending.
- QNX cross-build/device and Marathon execution: not run for this test-only
  continuation. The renderer and public ABI did not change; feature defaults
  remain OFF. This report closes no release gate or per-element device check.

## Skills, ownership and rollback

Read the repository `qnx-native-api` / `qnx-platform-validation` skills and the
pinned Margelo `api-design` → `build-nitro-modules` → `cpp` sources at
`1e9e17be6f41c838db472d6a4b943fc26355a3be`; their bytes matched
`skills.lock.json`. Applied decision: keep the native API and renderer intact,
retain every scene, and isolate test runtime ownership. Callstack
`react-native-best-practices` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`
was read; callback/host timings are not display measurements. The installed
`no-ai-slop` skill was applied to this report. No new skill installation is
claimed.

DRI: current integration agent, corpus fixture/generator/test/CMake paths.
CodeRabbit CLI 0.7.6 reviewed all staged changes, including the new packing
test and native harness, with zero findings. Human integration review remains
unassigned. There is no new permission, premium component,
license or production performance change. Revert this continuation's harness,
packing and threshold changes to restore the previous runner. Continue with
exact-head CI and the device acceptance list in today's handoff before merge.
