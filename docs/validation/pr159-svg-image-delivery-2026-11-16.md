# PR 159 SVG image delivery and provider lifetime

Validation ran on November 16, 2026 in America/New_York (November 17 UTC).
This slice consumes the separately reviewed rich `QtImageCache::loadImage`
completion and shared-memory epoch. Its own production changes are confined to
`QtRnsvgImageProvider.h` and `.cpp`, with real HTTP tests in
`RnsvgImageHttpTest.cpp`. The component table and SVG guide describe the resulting
behavior without promoting host evidence to current QNX support.

## Corrected behavior

The SVG provider now receives decoded pixels directly from the existing host
image loader. Previously it received only dimensions, then looked up pixels in
the shared cache. Valid private/no-store responses, an image over the memory
budget, or an in-flight load followed by a memory clear could therefore fail
merely because no cached entry remained. The new completion carries the `QImage`
independently of cache retention. Coalesced elements still share one request;
uncached responses stay uncached and a later request fetches again.

Completion can destroy a provider. `finish` removes the waiter list before
calling user code and checks a weak provider guard between deliveries. Later
waiters are suppressed after destruction. Invalid URL validation can also
complete synchronously inside `loadImage`; a guard spanning that call prevents
constructing a request handle from a destroyed QObject.

This keeps the Qt Quick/Fabric renderer and existing image loader. It introduces
no second renderer, new dependency, package pin, target flag or JS API. Direct
`data:` decoding retains its existing synchronous path. Caller-supplied headers
are still not forwarded by the SVG URI-only adapter. SVG document decoding
still depends on the appropriate Qt image plugin and target qualification.

## Native evidence

| Check                                                                                                      | Observed result                                                                                                   |
| ---------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Original provider from public PR 159 base with the new cache interface and the five initial behavior cases | All five fail: private/no-store pixels, budget miss, clear during fetch, and delivery after provider destruction. |
| Candidate with those five cases                                                                            | All pass.                                                                                                         |
| Final source with only the synchronous-return guard reverted, identical final dependencies and test        | Exit 1, failed missing-handle assertion and AddressSanitizer heap-use-after-free.                                 |
| Corrected final provider, all six cases                                                                    | Six cases pass; 8 QtTest entries including setup/cleanup, 481 ms.                                                 |
| Restored final complete SVG image HTTP suite                                                               | 1/1 CTest, 21 QtTest entries, no failures or skips, 1.65 seconds.                                                 |
| Current-delta QNX cross-build, Pi and Marathon shell                                                       | Not run.                                                                                                          |

The private/no-store cases use real local HTTP responses and verify visible
image dimensions, cache absence, request coalescing and a later refetch. The
budget case preserves delivery with a one-byte cache budget. The memory-clear
case holds the server response until after `clearMemory`, then checks that
pixels arrive while memory stays empty. The teardown cases exercise both
asynchronous coalesced waiters and actual synchronous invalid-URL validation.

The initial original-provider control uses the same rich-cache snapshot as its
candidate comparison. The later guard-only control deliberately keeps every
final dependency unchanged and reverts only the synchronous lifetime guard.
Its source is restored in a `finally` block before the final rebuild and full
suite. Source swaps update timestamps so Ninja recompiles the changed inputs.
No test assertion, timeout, feature gate or dependency pin is relaxed.

Host: Debian 13 x86-64, Qt 6.11.1, Clang 19.1.7, CMake 3.31.10, Debug build,
offscreen/software rendering. `RNQNX_QT_RNSVG_SANITIZERS=ON` instruments the
standalone image/renderer production sources and tests with AddressSanitizer
and UndefinedBehaviorSanitizer, with recovery disabled. The supplied Qt shared
libraries are not instrumented. Leak detection is disabled. The negative ASan
log retains addresses and its BuildId; the environment lacked an external
symbolizer, and no symbolic frames are inferred from a different binary.

## Reproduction and artifacts

```sh
cmake -S tests/native/qt-rnsvg-image-http -B build/svg-rich-image-asan -G Ninja   -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++   -DRNQNX_QT_RNSVG_SANITIZERS=ON
cmake --build build/svg-rich-image-asan --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1   ctest --test-dir build/svg-rich-image-asan --output-on-failure
```

The [source inventory](../evidence/pr159-svg-image-delivery-2026-11-16/source-validation.json)
records provider/test hashes, initial and final cache snapshots, fetcher state,
commands and the tested binary. Evidence includes
[configuration](../evidence/pr159-svg-image-delivery-2026-11-16/configure.log),
[original-provider build](../evidence/pr159-svg-image-delivery-2026-11-16/provider-control-build.log),
[original-provider failures](../evidence/pr159-svg-image-delivery-2026-11-16/provider-control-negative.log),
[guard control build](../evidence/pr159-svg-image-delivery-2026-11-16/guard-control-build.log),
[guard control failure](../evidence/pr159-svg-image-delivery-2026-11-16/guard-control-negative.log),
[final focused pass](../evidence/pr159-svg-image-delivery-2026-11-16/candidate-focused.log),
[restored final build](../evidence/pr159-svg-image-delivery-2026-11-16/candidate-build.log),
[final suite pass](../evidence/pr159-svg-image-delivery-2026-11-16/candidate-suite.log), and
[full CTest details](../evidence/pr159-svg-image-delivery-2026-11-16/candidate-ctest-details.log).
Configuration and sanitizer-log trailing whitespace is normalized without removing diagnostics.

## Review, limits and rollback

DRI: coordinating native runtime agent. Independent reviewer: separate Expo
image workstream agent. Review identified the synchronous callback/handle UAF;
the actual failure control reproduces it, and the final guard and three-file
source/test diff were approved with no remaining blocker.

The repository native API/platform-validation skills, locked Margelo pipeline
at `1e9e17be6f41c838db472d6a4b943fc26355a3be`, and Callstack guidance at
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4` informed owner lifetime, direct-value
completion, bounded worker reuse, and separate host/target evidence. Their
reading and decisions are documented in the preceding fetcher receipt. Review
is independent agent review; no new CodeRabbit run or device acceptance is claimed.

No permission, licensed payload, runtime attribution or target configuration
changes occur here. Cache-policy checks measure retention behavior, not device
performance or memory/latency budgets. The next step is integration with the
committed cache/worker APIs and exact combined-head validation. Current QNX
build, codec/plugin attribution, release application assets and device checks
remain pending; keep PR 159 draft. Rollback reverts this provider/test and guide
slice together with dependent cache API changes as needed, then rebuilds the
native application. No deployed state changed.
