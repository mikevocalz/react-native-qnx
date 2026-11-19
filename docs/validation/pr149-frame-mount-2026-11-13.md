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
Independent review: CodeRabbit CLI 0.7.6 reviewed the combined committed slice
`8154cae..0ecdaf7`, including native mount and gesture code, tests and receipts.
It found one stale documentation fragment; that link is corrected. No native
code finding was reported. The later Hermes importer change needs its own
review. Keep PR 149 draft while the remaining acceptance gates are open.
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
Next: finish gesture input and failure/teardown checks, then exact-head hosted
validation and independent review before promoting this draft.

## Gesture attachment continuation

Source inspected: RNGH 3.4.0 `v3/hooks/useGesture.ts` creates a handler with
empty config in a passive effect, then sends its full native config. Its
`configUtils.ts` sends `dispatchesReanimatedEvents`, filtering out
`shouldUseReanimatedDetector`. The platform had read the filtered property
only at creation and attached mounted detectors before handlers existed.

`DetectorBindings` retains detector and handler identities, accepts either
creation order, and refreshes the event route from the later native config.
It removes bindings when detectors disappear or handlers are dropped.
The real recognizer regression covers both orders, worklet/JS routing,
removal and tag reuse under Apple Clang ASan/UBSan; it passed together with
the fourteen existing recognizer cases. QNX adapter cross-build passed;
The Pi card now moves and, on release, reports translation 80,15 and springs
back. The first split-process release probe used a different injected device;
the complete drag in one injector invocation provides the valid release trial.
Single tap now shows count 1; the following double tap reports `double tap`
without incrementing the count. Independent review remains pending for this
continuation.
Native candidate SHA-256:
`5bfcdeeee1491293e627ecc56484b6c70a89e6e227141b2088319059cd8909c1`.
The HBC remains the readback candidate above.

The tap trace confirmed correct detector hit/routing, then both tap handlers
failed. A completed single tap retained its contact-duration deadline while
waiting for a double tap's delay; deadline failure also left idle handlers in a
terminal state until another pointer cycle. The regression using default timing
failed five checks before the fix. Clearing the satisfied duration deadline
and resetting idle handlers after timer processing passes all sixteen cases
under ASan/UBSan. Opt-in `RNQNX_GESTURE_TRACE` records hit geometry and routing;
leave it unset for normal use. The corrected tap device trial passed: count 1 after a single click, then
`double tap` with count still 1. Native hash:
`5a7cd77d8fc4b2c9ec7f4c2a0f0f7c033d10ff663bbb8990a3cd8a09ce9b7080`;
final Zustand-card HBC hash:
`eebd83c28838c572691b4b1dd8b919deaf48bb976e1e981bd2c8f8db8428f82a`.
The earlier card passed recognition but displayed an object because its React
functional updater was serialized across the worklet boundary. Its state now
uses a per-instance Zustand store; a plain RN callback increments taps locally.
The gesture-candidate source inventories cover 331 native/config files and
252 app/package files, including the new binding helper.

## Hermes import continuation

The physical trials above used the historical experiment's full VM behind a
lean-named build-tree alias. Their runtime observations remain valid for those
hashed artifacts, but that alias is not the install contract. The production
QNX importer now selects the real compiler VM when Worklets is enabled, as it
already does for development clients. The existing Worklets flag stays OFF by
default; ordinary release apps still select lean. The CMake fixture passes all
four flag combinations and rejects a missing full VM. Restoring the earlier
condition fails Worklets release selection. These are synthetic configuration
fixtures, not executable library or ABI tests. The real full-root cross-build passed, and the ELF names `libhermesvm.so`
as a required library. With the lean-named file removed from the test package,
the Pi completed the probe: thirty UI callbacks in 482 ms, worst interval
24 ms, distinct `gui`/`ui-worklet` roles. The busy-JS result remains 0/40.
Native SHA-256: `9303d1493c71a8e92c5c1db4928769c58accac46ce3551cec9bff791f18e857a`.
Full VM SHA-256: `a48934c6560d8da5adeb7fadb7469e4c02b9dd925846fdbb69bf36f78f96f62f`.
The bundle remains the final Zustand-card HBC recorded above. The owner tile
was restored through its guarded launcher after this trial. CodeRabbit reviewed
the importer, regression and accompanying records with zero findings.
