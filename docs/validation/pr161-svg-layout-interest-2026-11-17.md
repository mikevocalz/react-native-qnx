# PR 161 SVG layout subscriptions — November 17, 2026

This continuation starts from published head
`d3eacc75dcad761c992736223a68b53ff852203b`, tree
`fd7fe0ad6bcbd28e8c51d4a0526f1fadd2a543ea`, which matches local
`a75341a7b3509875e12605a5329c4e9ca202b69f`. The new local measurements below were
collected on November 17 in both UTC and America/New_York. They are Linux
host results. A fresh QNX build and current-device acceptance remain missing;
the PR stays draft and all eleven release gates stay pending.

## Problem and implementation

The previous published head completed ten successful workflows and one failed
renderer workflow. Renderer OFF passed 14/14 CTests. Renderer ON passed 47/49:
corpus shards 1 and 2 exceeded their unchanged 300-second limits at 300.11 and
300.12 seconds. Its later ON sanitizer and render steps were skipped after
that failure. Those results remain recorded in the PR and retained evidence;
host results from different heads or runners do not erase them.

The previous continuation guarded standard Fabric `onLayout` dispatch.
SVG-specific reporters still constructed and dispatched layout payloads for
every eligible element even when no React or native subscriber was present.
Hermes debug heap checks made this unnecessary event work costly in large
scenes. This change filters event dispatch while preserving the existing
Fabric, Hermes and Qt Quick renderer and all geometry work.

React Native 0.88.1's `ReactNativeAttributePayload` converts an event callback
to a true native marker and sends null when it is removed. Its native
`convertRawProp` retains the source value when an update omits the key and
uses the default on null. React Native SVG 15.16.0 forwards public `onLayout`
as `onSvgLayout`.

The existing generator adds a false-default Boolean marker to the native
schema for the fourteen components that already declare this event. Pinned
codegen emits the constructors, fields and serialization; generated C++ is
not hand-edited. Existing event metadata and production JavaScript overlay
specs are unchanged. QNX's BaseViewConfig admits the existing `onSvgLayout`
key so the normal React serializer can transport it.

Each reporter still observes bounds and updates its last rectangle. Only the
payload construction and dispatch are gated on the React marker or current
native interest. Adding a listener to unchanged bounds does not replay an
event. A later geometry change preserves the existing x/y/width/height payload.
Painting, image loading, retained item identity and standard onLayout remain
independent. This is a QNX subscription optimization, not a claim that upstream
RNSVG Fabric already has the same prop guard.

Native consumers can register after mount without a React callback prop. One
holder per mutation adapter therefore stores an optional runtime-free native
interest predicate. Reporters borrow it weakly, and the scheduler delegate
forwards the setter. Replacement and snapshot retrieval are synchronized;
invocation and final capture release occur outside the mutex. A QPointer is
checked after the snapshot and holder are released, covering a self-clearing
callback whose final capture destroys the reporter. The predicate is empty
by default. PR 149 owns the actual weak Reanimated-proxy registry integration
and its separate real-library evidence.

## Transport discovery and regression evidence

The first native-marker candidate passed generated prop parsing but timed out
waiting for the public SVG callback. Inspection found that RN's real
`ConditionallyIgnoredEventHandlers` includes event attributes only on iOS.
The generated QNX SVG config therefore omitted the marker. The old overlay
fixture's always-return stub had hidden this platform distinction.

The correction adds only `onSvgLayout: true` to QNX base validAttributes;
it adds no base direct-event metadata. The new test executes the pinned
codegen transform, real platform helper, `createViewConfig`, and real
AttributePayload create/diff functions. Identical test source fails with the
original QNX base and passes with the corrected base. It checks unchanged
Android/iOS helper and generated metadata behavior, stable-listener omission,
null removal and re-addition. The older overlay fixture now models the
requested platform correctly and retains its existing assertions.

| Evidence                                                           | Actual result                                                                                                                                             |
| ------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Original published production plus the public-listener fixture     | Five intended native-dispatch assertions fail in 7.06 s; geometry and JavaScript checks complete.                                                         |
| Initial native marker without QNX transport                        | Public callback times out; preserved as a failed intermediate candidate.                                                                                  |
| Identical real JS transport test against original/corrected base   | Exit 1 / exit 0, test SHA256 `72db22eb8dbc459cdae8e0d4b69c57c1c00a1cb0600dfd9ea28c671d4f0f4b41`.                                                          |
| Corrected public Metro/Hermes fixture and generated marker parsing | 2/2 CTests pass: 7.50 s and 0.05 s. All fourteen generated classes cover default, true, omitted, null, false and re-addition.                             |
| Complete public/native-interest fixture                            | Passes in 9.46 s: late installation, changing interest, self-clear, empty state, weak-owner expiration and reporter destruction on final capture release. |
| Explicit candidate lifetime-check mutation                         | With only the final QPointer check removed, the identical test object and bundle fail the intended extra-dispatch assertion in 7.89 s; CTest exit 8.      |
| Platform-aware overlay fixture                                     | 57 QNX probes and 57 iOS probes pass.                                                                                                                     |

The mutation is not labeled an original-source control. Its observable
failure is `Destroyed SVG reporter dispatched after capture release`.
The Fabric item and emitter remain alive, so the extra dispatch is detectable
without claiming whole-production AddressSanitizer coverage. The original
public control, initial transport failure, corrected candidate and explicit
mutation have separate logs, metadata and source/object/bundle identities.

## Complete candidate graph and timing

A separate clone was built from the exact published PR 161 source, with a
fresh build directory and the eleven existing ON renderer features. It uses
Qt 6.11.1, Clang 19.1.7, CMake 3.31.10 and the Debug preset. Seven dependency
archive hashes and 5,617 extracted regular-file contents were verified before
reusing only immutable dependency sources. No PR 149 compiled object or
library was reused. The cold build completed all 1,122 steps. Hermes remains
at `9a115f15785851f4ff2747ebb090ddd066fc8ad2` with slow debug checks enabled.

On this graph, the original heavy shard 1 passes in 199.36 seconds with
37 visual cases, zero pending/failures, and a 179,912 ms heavy-batch mount.
This is one local diagnostic; it does not revise the failed hosted run.

The restored candidate passes **49/49 CTests in 445.87 seconds**, including
all **57 combined QtReactHost cases** and **149/149 integrated SVG pixel
comparisons** (38 + 37 + 37 + 37), with zero pending or failed corpus cases.
The four shard times are **5.33 / 166.98 / 163.61 / 6.21 seconds**, under the
unchanged 300-second limits. The native-interest fixture passes again in
8.98 seconds and all generated marker sequences pass in 0.04 seconds.

The matched original/candidate shard-1 comparison is 199.36 versus 166.98
seconds, about 16.2% lower. Its heavy-batch mount is 179,912 versus 145,238 ms.
These are single local observations, not a general device performance claim
or a prediction of hosted runner timing. No other native build or test ran
concurrently. All reviewed source hashes remain unchanged through the run;
the restored reporter object, Fabric test object and public fixture bundle
are byte-identical to the earlier passing candidate.

The raw output retains the pre-existing nonfatal RN filter-conversion
diagnostics, also present in the original shard control. A passing graph is
not described as warning-free. The full log, JUnit output, test inventory,
configuration and final artifact identities are preserved.

The transferred candidate inputs initially retained older timestamps. That
was detected before accepting candidate test results, and the affected
objects were rebuilt. Compilation also caught four old `false` initializers
in filter table rows after the capability field became a function pointer;
they were corrected to `nullptr`, preserving those rows' lack of layout events.
Both diagnostics are retained.

Before the final graph, all 23 changed source/generator/test files matched
the reviewed primary clone byte-for-byte. The restored reporter source hash is
`20b46b497bb18cade48899742dbc70794cb0c5aa94555b9b0ba97dbf802faab3`.
The restored native build passes in 35.21 seconds. Nine generated application
bundles were inventoried because BaseViewConfig is not listed as a direct
dependency by every fixture. Output timestamp aging refreshed four; Ninja
restat retained five, so freshness verification stopped before CTest. The
exact existing Metro commands then refreshed those five. No source, fixture,
timeout or Hermes internal input was changed. Before/after hashes, timestamps,
commands and the initial freshness failure remain recorded.

The graph's existing ASan configuration instruments the Fabric test
translation unit and executable link. It does not instrument every production
translation unit or the prebuilt Qt libraries. Leak detection remains disabled
by the existing CTest properties. Prior standalone SVG ASan/UBSan results keep
their own scope; this continuation does not claim a new full-production
sanitizer or QNX run.

## Other checks, review and remaining acceptance

Dependency, architecture, TypeScript, pinned SVG codegen, documentation and
full-repository formatting checks are recorded with their actual commands and
exit statuses in the [final check log](../evidence/pr161-svg-layout-interest-2026-11-17/final-light-checks.txt).
The faithful transport, platform-aware overlay, resolver and package-packing
checks also retain their separate logs. The first documentation check requested
regeneration of the project evidence index; the standard docs generator refreshes
that index while retaining all gate entries. Its original failure and final
rerun remain in the check log. Published-head hosted checks remain separate
from these local checks.

The full local JavaScript runner encountered the existing Marathon development
launcher restriction, `listen EPERM` at `marathon-development-launcher.test.mjs:89`.
That command is recorded as environment-blocked, not passing. No test was
skipped or weakened. Published-head hosted CI must supply its complete result.

The SVG workstream agent implemented the slice. The coordinating root and
separate Expo integration agent independently reviewed the native ownership,
generated marker, transport correction and regression evidence. Root completed
the restored build, full graph and final report. These are agent reviews,
separate from external review services and target acceptance.

The repository `qnx-native-api` and `qnx-platform-validation` skills and pinned
Margelo/Callstack pipeline retain the exact provenance in the
[geometry receipt](pr161-geometry-2026-11-16.md#ownership-skills-and-review).
Applied decisions preserve public contracts, owner lifetimes, behavioral
controls, pinned codegen and the distinction between host and target evidence.
No new skill installation or fresh CodeRabbit review is claimed; the previously
recorded installer approval rejection remains unchanged.

No native dependency, license boundary, feature default, package pin, corpus
fixture, visual tolerance, test limit or debug check changes. The Linux host
uses the qnx JavaScript identity with software Qt rendering; it is not a QNX
Screen or Marathon Wayland runtime result. Current QNX cross-build/runtime
attribution, release-app animated-image assets and device receipts remain the
stack's acceptance work. Merge order is #161, #159, then #149 after the required
evidence exists. All eleven gate entries are unchanged, canonical SHA256
`ced2bf1a611f20f87eeccf058b7e484d98a0b1382aa165f18a69e07e36fe13b4`.

## Reproduction, evidence and rollback

On an equivalent prepared host, use the existing `linux-host-renderer` preset
with the recorded ON configuration, regenerate the application Metro outputs
when a transitive platform file changes, build, and run the complete CTest
preset. The evidence includes the exact commands, flags and original paths;
no diagnostic wrapper is part of production or the repository test graph.

All logs and machine-readable receipts are indexed in
[the evidence directory](../evidence/pr161-svg-layout-interest-2026-11-17/README.md).
The final source inventory and full-run metadata establish what was tested;
the eventual GitHub publication must match that reviewed tree and receive its
own hosted checks. A local pass is not a hosted or target pass.

Rollback removes this continuation's subscription marker, native-interest
holder/setter and dispatch gate, reverts the one base validAttribute, then
regenerates native props with the existing pinned generator. Keep the earlier
geometry and standard onLayout fixes. PR 149's optional registry wiring must
be reverted with the SVG setter if it has been integrated. Preserve the
evidence and restore any affected bundles through the normal build.
