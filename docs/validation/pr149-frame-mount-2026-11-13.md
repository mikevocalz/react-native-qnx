# PR 149 frame continuation and mount completion

Continuation of `5702675`, with main `f87c742` merged as `8154cae`.
Worklets 0.13.1, Reanimated 4.7.1 and Gesture Handler 3.4.0 retain their exact
locked versions. This slice preserves Fabric, Hermes and Qt Quick.

## Source inspection and changes

The Worklets Babel plugin transformed the nested frame callback separately,
capturing its own uninitialized binding and snapshots of mutable counters.
The callback is now a named local function created inside the outer worklet.
The regression executes the actual installed-plugin output through thirty
mocked callbacks; restoring the old callback makes that test fail.

Reanimated's mapper finalizer needs `maybeFlushUIUpdatesQueueFunction` to flush
its pending registry. The platform callback now weakly acquires its proxy and
calls `performOperations`. That change alone did not fix visible animation.
On the Pi, shared x advanced to 222.133 while pixels stayed at their initial
position until a React commit. The Qt scheduler never called `reportMount`.
The pinned Reanimated mount hook resumes paused animation commits there.
The delegate now reports a successfully applied mount through a weak UIManager
reference. Failed mutation batches are not acknowledged. A native host
regression observes item widths from a real UIManager mount hook after two
commits, proving callback ordering rather than checking source text.

The public-entry regression also caught RefreshControl returning its module
namespace; its getter now returns the default component. WorkletsProof and
ReanimatedCard component state use per-instance Zustand stores. Shared native
animation values remain in Reanimated.

## Evidence and limits

- Frozen pnpm 12.18.0 installation, typecheck, dependency, architecture and
  manifest checks passed. Public-entry and transformed-frame regressions pass.
- The full JS runner reached a Linux-only consumer configuration test and failed
  on macOS; this is not a full-suite pass.
- Physical Pi 400: fixed frame continuation delivered thirty callbacks in
  465 ms (worst interval 18 ms), then 472 ms (17 ms). GUI and UI-worklet roles
  were distinct. The busy-JS subtest delivered zero of forty worklets during
  its busy window, so that acceptance remains open.
- Frame-flush candidate QNX native SHA-256:
  `da3d254b57fe5c22ed30aed01117f3d5e65c19044e5638b075c05d4cebd63f7c`.
  It cross-built with SDP 8.0.5 but still showed the delayed-pixel defect.
- The native mount-hook regression passed in the Linux Qt 6.12.0 Release
  reproduction (sanitizers OFF). Its full host executable failed only
  `renderedTreeIsPainted` at the outer-red pixel assertion; the same failure
  persisted with the software backend. Disabling the mount-report callback
  makes the new hook test fail (`!hook.widths.empty()`), while retaining the
  same pixel failure. This transplanted test/source run is
  not an exact PR 149 full-host pass.
- Corrected QNX cross-build: passed, native SHA-256
  `f426e708b0d90b62db23e40ffbc669a7594c219ea6d057142b145300389a40d6`.
  The same readback HBC (`eb929592ce32eabbb05db39cef2d3cf8ee07212f8f5d3e573e17568992a4bb7b`)
  shows an intermediate box position/bar width while `timing running`, then
  final pixels while `timing finished`. The flush-only binary with that HBC
  stayed at its initial position until completion. Captures are in
  `docs/evidence/worklets-frame-2026-11-13/`; screenshot acquisition adds delay,
  so the requested 80 ms sleep is not an input-to-pixel latency measurement.
- Spring returns the box and reports completion. Injected pan produced no
  response; gesture acceptance and exact-head hosted CI remain pending.
- Candidate source inventories match all 330 native/config and 252 app/package
  files checked in the cross-build tree. The earlier inventories describe the
  preceding flush-only stage and remain as historical evidence.
- No Marathon, QNX VM, general conformance or release qualification is claimed.
  No boot files, window manager or app-window dimensions were changed.

## Skills, ownership and rollback

DRI: current integration agent for the listed runtime, test and gallery paths.
Independent review: CodeRabbit reviewed the earlier tracked frame/export slice
with zero findings. The completed native slice review hit its free quota
(wait reported: 18 minutes); it is not independently approved. Keep PR 149 draft.
Repository qnx-native-api and qnx-platform-validation were read and applied.
Margelo api-design, build-nitro-modules and cpp snapshots at
`1e9e17be6f41c838db472d6a4b943fc26355a3be` were read in that order and verified
against skills.lock.json. Decision: repair the existing upstream lifecycle
contract, use weak ownership and add an observer-based regression; introduce
no replacement native API. Callstack react-native-best-practices at
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4` informed the thread/pixel distinction.
The installed no-ai-slop skill was applied to this receipt.

Security/licensing: no new dependency, permission, premium payload or licensing
change. Performance budget is unqualified; these frame observations are bounded
samples, not latency percentiles. Roll back this slice's callbacks, tests and
gallery changes together; retain the original draft feature switches OFF.
Next: prove intermediate pixels without a React commit, then gesture input and
failure/teardown behavior before promoting this draft.
