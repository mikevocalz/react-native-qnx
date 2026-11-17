# PR 161 subscribed layout dispatch continuation

This continuation starts from public PR head
`3220c8df9aecf84d2019b3c9ca62d89c1b6f0ac7`, whose tree is identical to local
`cd2eb9a88900a5500eb074a5466826626f0eaf58`. Validation ran on November 16 in
America/New_York (November 17 UTC). It is host evidence for a Fabric event
dispatch correction. Fresh QNX compilation and device acceptance remain
required, and every release gate remains pending.

## Problem and bounded correction

The completed workflows at `3220c8d` include ten successes and one failed
full-renderer workflow. The renderer-OFF lane passed all 14 CTests. The
renderer-ON lane passed 47 of 49 tests; corpus shards 1 and 2 each exceeded
their existing 300-second limit. Their large Complex-1 and Complex-2 scenes
accounted for most of the time. The retained-geometry SVG test passed in the
same renderer lane. The [ON log excerpt](../evidence/pr161-layout-dispatch-2026-11-16/ci-on-3220c8d-excerpt.log)
and [OFF log excerpt](../evidence/pr161-layout-dispatch-2026-11-16/ci-off-3220c8d-excerpt.log)
retain the exact hosted job results. A later local run of the unchanged
validated PR 149 graph also took 295.30 and 273.25 seconds for those shards,
so the investigation did not attribute the cost to the new geometry fix.

`ShadowViewMutationAdapter::applyOne` previously invoked
`ViewEventEmitter::onLayout` on both creation and update whenever an emitter
existed. An emitter also exists when JavaScript has no `onLayout` listener.
The adapter now checks the existing `ViewProps::onLayout` boolean before
dispatch. Geometry application, emitter registration and other observer
callbacks retain their existing paths. This follows the pinned React Native
0.88.1 implementation in
`ReactCommon/react/renderer/uimanager/LayoutEventEmitter.cpp`, which checks
the same property before calling `onLayout`.

No public property, package dependency, renderer option, GC configuration,
test timeout, visual tolerance or corpus fixture changed. Hermes slow debug
checks remain enabled. The separate SVG-specific `onSvgLayout` path is
unchanged. The earlier retained-geometry implementation and its regressions
are unchanged.

## Profiling and scope of the experiment

The existing PR 149 host graph was fully built and idle after its 61-test
run. To avoid rebuilding or modifying that graph, the experiment copied its
adapter translation unit and archive, applied only the two listener guards,
and linked separate executables. The temporary adapter patch preserves the
branch's unrelated Worklets observer code. The final PR 161 source uses the
same guard in the same two mutation paths.

For diagnosis, a separate copy of the corpus test added elapsed-time markers
and a 10 ms CPU-time program-counter sampler. The signal handler records a
program counter into fixed storage; module and symbol resolution happens
after sampling stops. This identifies sampled modules and functions, not
call stacks. Of 25,574 original-run samples, 24,873 (97.3%) were in the shared
Hermes VM. Frequently resolved symbols belonged to heap-cell, segment-map
and card-table validation. The actual Complex-1 paint took 20.74 ms.

The pinned Hermes build has `HERMES_SLOW_DEBUG=ON`; its young-generation
collection checks heap/card invariants. The paired measurement is consistent
with those checks making unnecessary event allocations expensive. The source
contract and event-pump measurement support removing those events; the
sample does not establish a complete allocation call graph.

| Complex-1 phase                         |               Original adapter |                Guarded adapter |
| --------------------------------------- | -----------------------------: | -----------------------------: |
| Batch evaluation                        |                     136,940 ms |                     129,791 ms |
| First await event pump                  |                    53,447.5 ms |                        89.7 ms |
| Mount after the existing settling pumps |                     244,535 ms |                     181,239 ms |
| SVG paint                               |                       20.74 ms |                       17.66 ms |
| Shard visual outcomes                   | 37 passed, 0 pending, 0 failed | 37 passed, 0 pending, 0 failed |

The measured mount time fell by 25.9% in this paired diagnostic run. This is
one host comparison, not a statistical benchmark or a QNX performance
claim. The [original profile log](../evidence/pr161-layout-dispatch-2026-11-16/baseline-profile.log),
[candidate profile log](../evidence/pr161-layout-dispatch-2026-11-16/candidate-profile.log)
and [sample summary](../evidence/pr161-layout-dispatch-2026-11-16/profile-summary.json)
retain the observations. The [raw original PC counts](../evidence/pr161-layout-dispatch-2026-11-16/baseline-pc-samples.tsv)
and [sampler source](../evidence/pr161-layout-dispatch-2026-11-16/PcSampler.h)
make the diagnostic limits inspectable.

## Regression and compatibility validation

Two new real-Fabric host cases record both native `topLayout` dispatch and
the JavaScript payload. They cover creating and resizing without a listener,
subscribed creation and resizing, listener removal, readdition, exact
changed dimensions, and retention of the same Qt item. The recorder's
native counter outlives the fixture and its event listener.

The same compiled regression test object was linked once against the
original adapter archive and once against the guarded copy. The original
run passed all 55 existing cases and failed exactly the two new cases:
unsubscribed creation dispatched an event, and listener removal still
dispatched the changed bounds. It exited 1. The guarded host suite passed
all 57 cases, including the unchanged public React `viewLayoutFires`,
FlatList/SectionList, state-update, input and image cases.

The final corpus validation links the original, uninstrumented corpus test
object against the copied guarded adapter archive. It retains all four
shards, all 149 visual comparisons, their original fixtures and thresholds,
and each existing 300-second CTest timeout. All 149 comparisons passed with
zero pending or failed cases. The isolated run passed all five CTests in
518.93 seconds:

| CTest                                       | Passed cases | Duration | Corpus timeout |
| ------------------------------------------- | -----------: | -------: | -------------: |
| Host compatibility and listener regressions |           57 |  70.15 s |              — |
| Corpus shard 0                              |           38 |   5.21 s |          300 s |
| Corpus shard 1                              |           37 | 204.75 s |          300 s |
| Corpus shard 2                              |           37 | 232.22 s |          300 s |
| Corpus shard 3                              |           37 |   6.59 s |          300 s |

The [original host control](../evidence/pr161-layout-dispatch-2026-11-16/baseline-host-tests.log),
[candidate CTest summary](../evidence/pr161-layout-dispatch-2026-11-16/candidate-ctest.log),
[full test output](../evidence/pr161-layout-dispatch-2026-11-16/candidate-last-test.log)
and [JUnit result](../evidence/pr161-layout-dispatch-2026-11-16/candidate-ctest.xml)
retain the failures, successes, counts and timing. The original graph's
adapter archive and corpus executable hashes are unchanged after this run.
Only trailing whitespace and extra final blank lines are normalized in the
retained text logs; their diagnostics and results are preserved.

Host tools are Debian 13 x86-64, Clang 19.1.7, Qt 6.11.1, CMake 3.31.10,
Ninja 1.13.2, Node 26.13.1 and pnpm 12.18.0. Compile commands were copied
from `ninja -t commands` for the frozen Debug graph. The adapter uses its
existing strict warning flags and no sanitizer instrumentation. The host
test translation unit and link use ASan, with leak detection disabled; Qt
is an uninstrumented release build. This receipt does not claim full
ASan/UBSan coverage of the graph.

These copied-graph results isolate the native correction; they are not an
exact PR 161 full-graph run. The published successor needs its own required
workflows. The last exact hosted head remains `3220c8d` with the two corpus
timeouts recorded above until that successor finishes CI.

The [documentation check](../evidence/pr161-layout-dispatch-2026-11-16/docs-validation.log)
passes for all 24 components and 11 release gates after normal regeneration.
The [full pinned Prettier check](../evidence/pr161-layout-dispatch-2026-11-16/format-check.log)
also passes. The new dated handoff and evidence-baseline entry preserve every
earlier evidence entry and all eleven pending gates unchanged.

## Reproduction and review

The [source inventory](../evidence/pr161-layout-dispatch-2026-11-16/source-validation.json)
records final PR 161 source hashes, the original and copied adapter hashes,
the identical host-test object, original graph artifact hashes, compile
commands and instrumentation limits. The
[experiment patch](../evidence/pr161-layout-dispatch-2026-11-16/experiment.patch)
contains only the two native guards. The diagnostic build scripts are
retained with their original working paths:

- [Build the separate profile](../evidence/pr161-layout-dispatch-2026-11-16/build-profile.py).
- [Build the guarded adapter and paired host tests](../evidence/pr161-layout-dispatch-2026-11-16/build-listener-candidate.py).
- [Link the original corpus object and create isolated CTest entries](../evidence/pr161-layout-dispatch-2026-11-16/build-candidate-validation.py).
- [Inspect the unchanged CTest shard limits](../evidence/pr161-layout-dispatch-2026-11-16/CTestTestfile.cmake).

On an equivalent configured host graph, the ordinary repository targets
remain `qt_react_host_host_test` and `rnqnx_qt_rnsvg_corpus_test`; build them
with at most two jobs and run the host case plus all four corpus CTests.
No profiling helper is part of production or the repository test graph.

The SVG/Fabric workstream owns this correction. The separate QNX readiness
agent and coordinating root agent independently reviewed the final native
guard, both regression cases and the copied experiment delta, finding no
blocking source issue. Their approval remains separate from hosted CI and
target acceptance.

The repository `qnx-native-api` and `qnx-platform-validation` skills and the
locked upstream pipeline recorded in the
[geometry continuation](pr161-geometry-2026-11-16.md#ownership-skills-and-review)
were applied: preserve the existing public contract, use native source
ownership and lifecycle boundaries, demonstrate an original-source failure,
retain unchanged acceptance checks, and keep host and target evidence
separate. No target connection, deployment, cross-build or device result is
claimed by this receipt.
