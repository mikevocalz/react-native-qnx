# PR 149: Reanimated native SVG event interest

Date: 2026-11-17 UTC. Linux Qt/Hermes/Fabric host evidence.

**The restored native subscription hook passes both configured Worklets CTests.**
The same fixture with the previously committed Reanimated module renders the
changed SVG geometry, then fails to receive the late-registered event. This is a
control of the module integration against the new SVG admission code, not a run
of the original whole branch.

The control and initial candidate sections below describe the **provisional
source snapshot** tested before the prerequisite was published. The final
integration replaces those patches with actual published PR 159 head
`12cae67885b77f9a371059fc2b0a1168ef07430b`. Its separate final-parent comparison
and 2/2 configured test result appear below; the earlier tests retain their
original provenance.

## Source identity

The local parent is `ff86386a218ceda4e842e79c601fd70c2ff43b7a`. Its reviewed tree
`c0b0efed3d7067b79edfc586053ae7f47ad354a8` is identical to the published PR 149 head
`75782e716d3a17fb7a22c47f883e37585a94f640`. The published head's completed CI is
separate evidence; it predates this continuation.

The provisional prerequisite consists of three reviewed SVG patches:

| Patch                                                                       | SHA256                                                             |
| --------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| Native listener marker, synchronized interest holder, renderer and fixtures | `802fd7d69f5afe66d78808f3b46f0db45cd8aef352c64b44f1cc4f963b1edbd0` |
| Four filter-table absent-callback sentinels changed to `nullptr`            | `8584890b86286aa2b56dd331fcd444a41550430964369483a222208a1ebe9bc1` |
| QNX marker transport and faithful JavaScript pipeline regression            | `ab3bf560ce92ae0a96f684e4b5092644655a09fd4151e73694d47fff9f471eef` |

The 23 prerequisite files remain distinct from the three PR 149 continuation
files: `QnxReanimatedModule.cpp`, `WorkletsIntegrationHostTest.cpp`, and
`worklets-integration.js`. Bounded merge resolution retains the existing modal
registration, view observers, owner-thread hooks, and QNX click attributes.
The added BaseViewConfig entry is only the existing `onSvgLayout` attribute.

[Source identities](source-identities.json) record all 26 actual source hashes,
the frozen prerequisite manifests, and the five files that retain existing
PR 149/159 context. The [combined patch](combined-source.patch) is relative to
the local parent and has SHA256
`3f32bac4cd2e55fbc5ceba1b18708d2923243baf0cad21a248d8ff249dfb44b4`.
The [control](control-source-identities.json) and
[candidate](candidate-source-identities.json) manifests differ in the
Reanimated module source only.

## Behavior and ownership

With both optional features enabled, the Reanimated module installs a per-host
predicate through `QtSchedulerDelegate::setSvgLayoutNativeEventInterest`.
It captures a weak proxy and queries
`isAnyHandlerWaitingForEvent("onSvgLayout", tag)`. The pinned registry protects
this tag-specific lookup with its mutex; the query does not access JSI values.
The synchronous call takes a temporary strong reference to the proxy.

The SVG prerequisite retains its reporters, bounds tracking, and last observed
rectangles. After a bounds change it dispatches when either the React listener
marker or the current native-interest predicate admits the event. Replacing the
predicate is synchronized, and its immutable callback snapshot is invoked and
released outside the holder lock. Existing mounted nodes therefore observe
later registration and removal without requiring a React prop update.

Invalidation clears the predicate before proxy cleanup and reset, while the host
and owner runtime still exist. The same SVG feature guard surrounds installation,
clearing, and the delegate method. Defaults and dependency pins are unchanged.

## Actual library regression

The fixture imports the pinned Worklets, Reanimated, Gesture Handler, React
Native, and SVG packages through Metro. Its SVG case deliberately calls the
actual tag-specific native registration interface directly, after a plain
`Rect` has mounted without a React layout listener. This exercises native
subscription behavior independently of a React prop marker. It does not qualify
every public `useEvent` or animated SVG composition.

Each of three fresh host instances verifies:

1. Initial green geometry renders without a native SVG dispatch or worklet
   callback.
2. A late registration completes through the real UI queue. Registering does
   not replay unchanged bounds. Increasing width from 10 to 20 changes actual
   painted pixels and delivers one event in the UI worklet runtime, with the
   exact `{x: 0, y: 0, width: 20, height: 20}` layout.
3. Unregistration completes before width changes to 30. Geometry still paints,
   and both native dispatch and JavaScript callback counts remain unchanged.
4. Re-registration does not replay the width-30 box. Width 40 paints and
   produces exactly the second event, with the corresponding layout.

UI FIFO barriers wait for asynchronous registry mutations and event work. An RN
queue barrier follows the UI barrier before assertions about absence of
callbacks. Native dispatch counters distinguish a suppressed native dispatch
from an event that merely lacks a JavaScript callback.

The first two hosts retain the existing frame round trip, real timing/pan
transforms, rendered pixel checks, and unmount tests. The third host leaves a
handler that has actually delivered events installed before scheduling a real
library RAF callback. The existing admission value assertion proves that the
worklet ran and that its frame callback remains pending when teardown begins.

## Control and candidate results

| Run                                                           | Actual result                                                                                                                                                                                           | Preserved evidence                                                                                                                          |
| ------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| First direct original-module invocation                       | Width-20 geometry completed, followed by the expected five-second missing-subscriber failure. LeakSanitizer then reported its separate ptrace restriction. Exit 1, 11.51 seconds in the native process. | [Log](control-direct-lsan-run.log), [result and artifact hashes](control-direct-lsan-result.json)                                           |
| Same original-module control through the configured CTest     | The geometry marker completed and only the late native subscriber check failed. CTest exit 8; one test failed in 12.94 seconds, total 12.95 seconds. No LeakSanitizer fatal diagnostic.                 | [CTest](control-ctest.log), [native output](control-last-test.log), [JUnit](control-ctest.xml), [result](control-ctest-result.json)         |
| Restored reviewed module, unchanged fixture and prerequisites | **2/2 CTests passed**: real integration 16.60 seconds; missing-bootstrap rejection 2.13 seconds; total 18.74 seconds. All three SVG trials emitted the late/remove/readd success marker.                | [CTest](candidate-ctest.log), [native output](candidate-last-test.log), [JUnit](candidate-ctest.xml), [result](candidate-ctest-result.json) |

The original-module control uses the module source from the local parent, whose
published equivalent is identified above. Its SHA256 is
`fb4c442c4f9b60b73c6f215143162efb9382623fee37605b83df7af370d295c3`;
the restored module is
`f92c5c0db0abe2832f21b25b2862010ffb34eab33cfc534d306c3f66f1d284ef`.
The SVG dispatch guard remains unchanged in both runs.

The same Metro bundle was used for both control and candidate, SHA256
`0683a1188a6f38bc97dadc462994503f707f2f6ff1ee4924f65721da0a18c752`.
The candidate rebuild recompiled the module and relinked the target; it did not
change the fixture. Both build commands completed successfully, with raw logs
for the [control](control-build.log) and [candidate](candidate-build.log).

## Commands and instrumentation

The target was built for each module source with:

```sh
cmake --build build/linux-host-renderer \
  --target rnqnx_worklets_integration_host_test --parallel 3
```

The configured control used:

```sh
ctest --test-dir build/linux-host-renderer --no-tests=error \
  --output-on-failure --output-junit control-ctest.xml \
  -R '^rnqnx_worklets_integration_host_test$'
```

The restored candidate used:

```sh
ctest --test-dir build/linux-host-renderer --no-tests=error \
  --output-on-failure --parallel 1 --output-junit candidate-ctest.xml \
  -R '^rnqnx_worklets_(integration_host_test|rejects_missing_bootstrap)$'
```

JUnit destinations in the actual commands were absolute paths in this evidence
directory; complete argument arrays are retained in the result JSON files.
The [generated test properties](test-properties.json) retain the existing
60-second per-test limit, software Qt rendering, and
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`. No configured check was relaxed.
The first direct run did not use that CTest environment and is retained rather
than treated as clean sanitizer evidence.

The [environment snapshot](validation-environment.json) records Debug,
`HERMES_SLOW_DEBUG=ON`, and the actual installed packages: React 19.3.0,
React Native 0.88.1, Expo 58.0.24, SVG 15.16.0, Worklets 0.13.1,
Reanimated 4.7.1, and Gesture Handler 3.4.0. The host uses Qt 6.11.1 and
Clang 19.1.7. Native compilation and execution occupied a serialized team slot.

[Actual target compile commands](target-compile-commands.json) show AddressSanitizer
on the fixture translation unit, with the existing link instrumentation.
The Reanimated module and all upstream/Qt objects are not thereby instrumented.
These two tests do not establish whole-graph ASan, UBSan, or leak coverage. The
configured runs produced no AddressSanitizer diagnostic. No QNX cross-build or
physical target execution occurred in this continuation.

## Review and remaining qualification

Independent source review covered the weak-only capture, normalized event name,
registry synchronization, feature guards, clear-before-reset ordering, FIFO
barriers, and listener lifetime. The parent integration review also checked the
bounded adapter/delegate merge and retained modal and view-observer behavior.

This receipt establishes the exercised Linux host native event path and teardown
scope. Animated SVG prop coverage, broader public hook compositions, dedicated
RN JS-thread work, current physical QNX execution, and release qualification
remain separate acceptance work. All 11 formal release gates remain pending.

## Final actual-public-parent integration

The final merge uses published PR 159 commit
`12cae67885b77f9a371059fc2b0a1168ef07430b`, tree
`b057996aeb770ce1b1349077a0106c85d3f4ff4a`. Its parents are the previous Expo
head `d50e34d99a504f48b690225ad28e8cb3cd8ca37a` and published SVG continuation
`6f40a8d5a26a987ebfe277b9e7d7a0f051d4d58c`.
The [public-parent record](final-public-parent.json) and
[fetch log](fetch-public-parent.log) retain that identity.

The 23 provisional prerequisite paths were restored to the local parent before
merging the real public commit. The three PR 149 files were preserved separately.
Native and application source merged automatically. Only the handoff, canonical
readiness evidence and generated project index conflicted. Both parent evidence
maps and dated handoff sections remain intact; the project index was regenerated.
The complete eleven-gate array equals both parents, with every status pending.

The [source comparison](final-source-comparison.json) verifies all 26 resulting
source hashes against the provisional candidate, all three package/lock/Worklets
CMake hashes, and all 25 recorded build settings. Every comparison matches.
Two independent reviewers rechecked the source, pin/configuration and metadata
unions. No new source fix was needed during the final run.

The ordinary target rebuild succeeds in **72.545 seconds**. The final configured
CTest pair passes **2/2 in 20.10 seconds**: actual-library integration takes
17.85 seconds and missing-bootstrap rejection takes 2.25 seconds. This run uses
the same commands shown above, with `final-ctest.xml` as the JUnit destination.
The [build](final-build.log), [build result](final-build-result.json),
[CTest output](final-ctest.log), [native output](final-last-test.log),
[JUnit](final-ctest.xml) and [result with artifact hashes](final-ctest-result.json)
record this actual-parent run separately. The [test properties](final-test-properties.json)
are identical to the provisional properties. All source hashes remained unchanged.
The rebuilt executable, Reanimated archive and Metro bundle are byte-identical
to the provisional candidate; final rendered frames are retained in `final-frames/`.

Only these two affected native tests were rerun. The original-module controls,
SVG prerequisite's 49/49 graph and previous PR 149 head's hosted 61/61 graph keep
their own source identities. The new published PR 149 head still needs its own
hosted workflows. This continuation does not supply new QNX cross-build or
physical-device acceptance, dedicated JS-thread work, general animated SVG
props or remaining recognizer/layout/scroll qualification.

The [validation summary](../../validation/pr149-svg-native-interest-2026-11-17.md)
links this evidence to the current handoff. Documentation generation, validation,
formatting and final integrity are recorded in the corresponding `final-docs-*`,
`final-format-*` and `final-integrity.json` files in this directory. Raw logs,
patches, JUnit and PNGs retain their bytes; JSON records may be whitespace
formatted with equivalent parsed values, recorded by `evidence-copy-identities.json`.
