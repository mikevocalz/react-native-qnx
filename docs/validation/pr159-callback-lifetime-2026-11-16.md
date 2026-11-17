# PR 159 callback capture lifetime follow-up

Validation ran on November 16, 2026 in America/New_York (November 17 UTC).
This follows [fetch admission validation](pr159-fetcher-admission-2026-11-16.md)
at local commit `69248f969aa0602473059c293151de1d08b3e58b`.

## Behavior and architecture boundary

A completed image request now releases its callback captures before admitting
another queued request. The earlier admission fix invoked the completed callback
first, but retained that callback inside shared `Pending` state until after the
queue started a successor. That successor can destroy the runtime. A retained
JSI-facing capture must not be released only after that destruction.

`complete` moves the completion out of `Pending`, clears progress state, and
uses declaration order to destroy the local completion before the native
admission guard releases its slot. Other native stack frames may still retain
`Pending`; they no longer retain the completed callbacks. Cancellation finishes
all owner mutations, clears both callback objects, then releases admission using
native-only local state. Running file I/O still keeps its admission until the
worker actually finishes.

The change is confined to `QtImageFetcher.cpp` and its native unit tests.
It preserves the asynchronous API, physical worker bound, priorities and
owner-thread delivery. It adds no dependency, changes no target flag, and
performs no device operation.

## Evidence

| Check                                                   | Observed result                                                                                    |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| Source at `69248f9`, with both final regressions        | Exit 2; completion and active-network cancellation each retain callback captures into a successor. |
| Corrected source, identical regressions                 | Exit 0; both pass, 4 QtTest entries including setup/cleanup, 57 ms.                                |
| Complete image units, view and queue suites             | 3/3 CTests, 77 QtTest entries, no failures or skips, 11.69 seconds.                                |
| Current QNX cross-build, physical Pi and Marathon shell | Not run.                                                                                           |

The completion regression queues a request whose immediate timeout callback
destroys the fetcher. It verifies that the first request completes with its owner
alive and that its weak callback capture expires before the successor begins.
The cancellation regression uses a real active HTTP reply, retains distinct Done
and Progress captures, cancels it, and verifies both captures expire before a
queued successor destroys the fetcher. Neither test relaxes timeouts or assertions.

The existing standalone Debug configuration uses Qt 6.11.1, Clang 19.1.7,
AddressSanitizer and UndefinedBehaviorSanitizer. Production sources and test
executables are instrumented; the prebuilt Qt libraries are not. Leak detection
is disabled. The source inventory records the concurrent decoded-memory cache header
present in both controls; that separately owned file is not in this commit.

The [source inventory](../evidence/pr159-callback-lifetime-2026-11-16/source-validation.json)
contains exact source and binary identities, commands and results. Raw evidence:
[control build](../evidence/pr159-callback-lifetime-2026-11-16/control-build.log),
[control failures](../evidence/pr159-callback-lifetime-2026-11-16/control-negative.log),
[candidate build](../evidence/pr159-callback-lifetime-2026-11-16/candidate-build.log),
[focused pass](../evidence/pr159-callback-lifetime-2026-11-16/candidate-focused.log),
[suite pass](../evidence/pr159-callback-lifetime-2026-11-16/candidate-suite.log), and
[full CTest details](../evidence/pr159-callback-lifetime-2026-11-16/candidate-ctest-details.log).

## Review and remaining gates

DRI: coordinating native runtime agent. Independent reviewer: separate SVG
workstream agent; no blocking findings in the final two-file source diff.
The repository native API/platform-validation skills and pinned Margelo and
Callstack guidance are recorded in the preceding receipt. They require native
worker values, owner-thread callback destruction, meaningful failure controls,
and separate host/device evidence. This is independent agent review; no new
CodeRabbit run or physical-device acceptance is claimed.

No licensing, sandbox or platform-identity boundary changes. The same bounded
admission policy remains in force; no frame-rate or target latency measurement
is inferred from these tests. Next: integrate the reviewed cache and worker
slices, then validate the combined PR. QNX/runtime asset attribution and device
gates remain pending, and the PR remains draft. Rollback reverts this two-file
follow-up and rebuilds the native application; no deployed state changed.
