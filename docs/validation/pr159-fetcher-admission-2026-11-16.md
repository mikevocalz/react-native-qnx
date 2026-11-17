# PR 159 image fetch cancellation and admission

Validation ran on November 16, 2026 in America/New_York (November 17 UTC).
The public comparison base is `e032a5b2c946c0de4b899daebed73bafed94108d`.
The isolated checkout also contains the separately reviewed explicit-write
generation correction; its fetcher production sources are identical to that
public base. This receipt covers only the fetcher changes described here.

## Corrected behavior

Cancelling or destroying an image fetcher already suppressed its callbacks,
but it released the shared admission slot while its file or disk worker could
still be running. Repeated cancellation could therefore exceed the queue's
configured physical I/O limit. A deterministic original-source test keeps
two real file reads blocked in empty FIFOs, destroys their fetcher, and shows
a third image completing despite the two-slot limit.

Each started request now has native admission state. A cancelled request drops
its owner callbacks immediately while its watcher keeps the slot until the
worker finishes. The watcher has a weak owner reference and does not retain
JSI values or the request's completion callback. The private queue has shared
ownership so it survives fetcher destruction. An external queue and worker
pool must outlive pending work and owner-thread completion processing.

Completion also releases admission after calling the completed request's
callback, with an exception-safe scope guard. Previously, queue pumping could
start an immediately failing request whose callback destroyed the fetcher
before the original completion callback ran. A separate regression reproduces
that ordering and checks that the completed callback still has its owner.

The implementation preserves asynchronous fetching, priorities, redirect and
header policies, byte limits, disk generations and the public JavaScript API.
It does not interrupt a blocking native file read. Cancellation suppresses its
result, and its existing slot remains occupied until the read returns.

## Executed validation

Host: Debian 13 x86-64, Qt 6.11.1, Clang 19.1.7, CMake 3.31.10 and the
repository's existing offscreen/software Qt Quick backend. The standalone
image production sources and test executables were built in Debug with
AddressSanitizer and UndefinedBehaviorSanitizer, frame pointers,
`-fno-sanitize-recover=undefined` and the unchanged strict warning policy.
Qt's prebuilt libraries are not instrumented. Leak detection is disabled.

| Check                                                         | Observed result                                                                                                                                                                                    |
| ------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Original fetcher source and final four focused tests          | Exit 3: premature release, callback-after-owner-teardown ordering, and third physical image admission all fail as intended. The private-queue lifetime case passes on the original implementation. |
| Corrected source with identical four tests                    | Exit 0: all four cases pass; six QtTest entries including setup/cleanup, 219 ms.                                                                                                                   |
| Complete image units target                                   | 32 QtTest entries pass.                                                                                                                                                                            |
| Complete Expo image view target                               | 31 QtTest entries pass.                                                                                                                                                                            |
| Complete fetch queue target                                   | 13 QtTest entries pass.                                                                                                                                                                            |
| Combined affected suite                                       | 3/3 CTests, 76 QtTest entries, no failures or skips, 11.59 seconds.                                                                                                                                |
| Current-delta QNX cross-build, physical Pi and Marathon shell | Not run.                                                                                                                                                                                           |

The focused coverage checks cancellation while a worker is queued, private
queue lifetime after owner destruction, immediate callback-capture destruction
on the owner thread, completion reentrancy, and actual blocked file reads.
The FIFO test is compiled on Unix hosts, where its native primitive is
available; it executed on this Linux host. Its cleanup closes both writers,
joins the pool and processes owner events while the external queue remains
alive, including assertion-failure paths.

The original-source control was run by restoring both fetcher production files
from the public base, rebuilding against the final test file, recording the
three failures, and restoring the candidate in a `finally` block. The final
candidate rebuild and complete affected suite followed that control. No
assertion, timeout, gate, dependency pin or feature default was relaxed.

## Reproduction and artifacts

With the pinned Qt prefix and compiler on the normal environment paths:

```sh
cmake -S tests/native/qt-native-ui -B build/fetcher-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build/fetcher-asan --parallel 2 \
  --target rnqnx_image_units_test rnqnx_image_fetch_queue_test rnqnx_expo_image_item_test
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/fetcher-asan \
  -R '^(rnqnx_image_units_test|rnqnx_image_fetch_queue_test|rnqnx_expo_image_item_test)$' \
  --output-on-failure
```

The [source inventory](../evidence/pr159-fetcher-admission-2026-11-16/source-validation.json)
records original/candidate blob and SHA-256 identities, the dependency lock,
commands, outcomes and tested binary hashes. Raw evidence includes
[configuration](../evidence/pr159-fetcher-admission-2026-11-16/configure.log),
[original build](../evidence/pr159-fetcher-admission-2026-11-16/baseline-build.log),
[original failures](../evidence/pr159-fetcher-admission-2026-11-16/baseline-negative.log),
[final candidate build](../evidence/pr159-fetcher-admission-2026-11-16/candidate-suite-build.log),
[focused pass](../evidence/pr159-fetcher-admission-2026-11-16/candidate-focused.log),
[suite pass](../evidence/pr159-fetcher-admission-2026-11-16/candidate-suite.log),
and [complete CTest output](../evidence/pr159-fetcher-admission-2026-11-16/candidate-ctest-details.log).
Configure-log trailing whitespace is normalized; diagnostics are retained.

## Ownership, source guidance and remaining gates

DRI: the coordinating native runtime agent. Independent review: the separate
SVG workstream agent. Its initial review found an assertion-failure cleanup
ordering issue in the test; the external queue now outlives the cleanup and
completion drain. Final review found no remaining source blocker. Review is agent source review, distinct from CodeRabbit or
device acceptance. A new CodeRabbit run was unavailable because automatic
approval review rejected the installer's telemetry connection with an
unverified payload; the rejected installer was not retried.

Repository `qnx-native-api` and `qnx-platform-validation` guided owner-thread
cleanup, lifecycle regression selection, exact source evidence and separation
of host and target results. The locked Margelo sources were read in order:
`api-design`, `build-nitro-modules`, then `cpp` at
`1e9e17be6f41c838db472d6a4b943fc26355a3be`. Callstack's
`react-native-best-practices` was read at
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4`. They informed the bounded native
work admission, owner-only callbacks and retention of plain values after
teardown. No personal skill installation or Nitro binding generation is
claimed.

This uses existing Qt Core APIs and adds no dependency, renderer, premium
payload, licensed binary, permission bypass or target operation. The measured
resource property is the two-worker test admission bound; no frame-rate,
latency or physical-device performance qualification follows from it.

The next integration is the image worker/cache continuation and then the
current combined PR host graph. Fresh QNX SDK compilation, runtime attribution,
release-app asset checks and actual device acceptance remain required. Keep
the PR draft and all release gates pending until their evidence exists.
Rollback reverts the fetcher admission changes and corresponding tests, then
rebuilds the native application. No device state was changed.
