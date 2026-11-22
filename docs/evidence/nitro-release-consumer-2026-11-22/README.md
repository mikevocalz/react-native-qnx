# Patched Nitro core in a standalone consumer

This is host-only packaging evidence for the concrete Release receiver crash
fix. It does not qualify QNX execution, physical hardware or release gates.
The native patch/regression is reviewed separately.

The platform already pins Nitro core to
`1.0.0-alpha.nightly.136.gdb81de61bd32`. Its reviewed patch now travels through
root pnpm policy and the CLI's exact four-patch inventory. The patch SHA-256 is
`36a3049c6cac9b38d2a74d37039c2052b3c7adf91c9357c289a152e1d3acc150`.
A generated app must install this same existing infrastructure version: copying
the patch without core caused an actual `ERR_PNPM_UNUSED_PATCH`. The failed
attempt was retained separately; unused-patch enforcement remains enabled.
No dependency version was upgraded and no experimental native default changed.
Nitro Forge remains outside the generated dependency graph.

A new external app, `fresh-consumer-nitro-release-pinned`, was generated through
`project create`; its own lock was resolved and then frozen-installed with
pnpm 12.21.0. Previous consumer proofs and lockfiles were preserved. The
[receipt](consumer-receipt.json) binds the reviewed source, root graph, all three
actual vendor archives and consumer files. Both repository and consumer installed
headers match the patched SHA-256
`9d8f3e405784bfc11a32c5226810491c02a54fe0f6d842e829be3bbca34119d8`.

Both the app's production bundle script and the actual `project bundle` command
succeeded; the latter produced 1,444,326 bytes. The real Expo CLI
[development probe](dev-result.json) served QNX, observed source edits, restored
the identical initial bundle hash, received the actual SDK v2 reload broadcast
on a synthetic host WebSocket and stopped its owned server. This does not prove
native client delivery or on-device Fast Refresh.

Two actual `project prepare-native` runs succeeded. Their
[record](prepare-twice.json) verifies identical generated descriptor and icon
hashes, and unchanged package, lock, app configuration and application source.
Native discovery finds no Nitro or Expo module requirement. The four existing
Fabric peer requirements remain: Gesture Handler, Reanimated, Safe Area Context
and Screens. The actual `analyze` command correctly reports stock-runner support
as undetermined without a runner inventory. Adding core does not make it a native
module: preparation omits `nitroModules`, and native build enables Nitro only
for a nonempty explicit module inventory. No runtime capability is inferred
from the installed header. ABI comparison against the checkout's existing
runner metadata passes; this is a metadata check, not compiled-Nitro proof.

The actual registry [audit](registry-audit.json) exits zero with no advisories
across 527 dependencies (489 regular and 38 optional). Generator/archive checks,
consumer doctor fixtures and installed checks, integrated QNX/Marathon Metro
checks, and 67 runner compatibility checks passed. Formatting passed.

Raw create/install/bundle/analyze/prepare/server logs and the unused-patch
negative control are preserved under
`/Volumes/QNX/clones/rnqnx-software-20261122/expo/nitro-release-*`.
Rollback requires reverting the reviewed native patch, root patch map/lock and
CLI pin/inventory together; do not drop the patch while retaining its runtime.
