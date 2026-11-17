# PR #159: image computation workers and cache lifecycle

Date: 2026-11-16 America/New_York (2026-11-17 UTC). This receipt covers host work following PR #159's
`e032a5b2c946c0de4b899daebed73bafed94108d` baseline. It does not supply the
integrated candidate's QNX build, plugin/attribution audit, release animation
assets or exact-device receipt. Short follow-up hashes identify local review
commits; the source and binary hashes in the evidence bind each validation run.

## Changes

The explicit cache writer now captures the disk generation before fetching its
source. A disk clear invalidates that writer even if fetching or decoding has
not finished. The owner completion also checks the generation after the worker
has committed the file, before publishing a decoded alias.

`ImageTaskQueue` runs native computations through the shared image admission
queue. Workers hold native values; Promise, registry and event callbacks remain
on the QObject owner thread. Destroying an owner drops its queued work and owner
callbacks. A running worker keeps its admission slot until it physically ends.
The completed callback is destroyed before admission starts another task, which
can itself destroy the owner.

The ImageRef operations move decoding, resizing, tinting, BlurHash/ThumbHash
encoding, PNG encoding and disk reads/writes to those workers. Native image
snapshots keep admitted operations valid after their JavaScript source is
released. `releaseImage` and `configureCache` retain their synchronous contracts;
applying a disk budget can still evict files synchronously.

`QtImageCache` also decodes on the worker queue. Its internal rich completion
delivers pixels and original dimensions independently of retaining an entry in
the decoded cache. The separate SVG provider change consumes those pixels for
private/no-store responses, budget misses and concurrent memory clears.

The shared decoded cache has a memory generation. Core loader requests, Expo
view requests and explicit alias writes capture it at admission, and insertion
checks it under the same mutex as a memory clear. Pending loads still display
their pixels or return their results, but do not refill a cleared memory cache.
Memory clearing leaves the disk generation unchanged.

Disk clearing admits its generation immediately and sweeps files on a worker.
Reads and newer writes apply any pending sweep before using the disk cache, so
a delayed clear cannot remove a newer successful write. Unsuccessful sweeps
remain pending and return `false`. `getCachePath` checks the filesystem on the
worker queue and resolves a path or `null`.

## Deterministic controls

The [evidence directory](../evidence/pr159-image-workers-2026-11-17) contains the
raw test output and binary/source identity records. Controls are named for the
actual source or mutation used; a mutation is not described as an original
source run.

| Behavior                                       | Control result                                                                                                                                                                                                 | Candidate result                                                                                                                                                     |
| ---------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Clear during an explicit source fetch          | Original `e032a5b` loader source with the new regression fails all three stale-write assertions: rejection, disk removal and decoded-alias suppression.                                                        | Generation fix `0cf65e6` passes both real-Hermes Expo module and Fabric executables.                                                                                 |
| Physical worker admission after owner teardown | The premature-release utility mutation reaches three simultaneous physical workers with a two-slot queue.                                                                                                      | Repeated teardown keeps the measured peak at two and allows a successor after those workers finish.                                                                  |
| Callback lifetime before successor admission   | The first utility implementation fails the weak-capture-expired assertion when admission starts a successor that destroys the owner.                                                                           | The corrected utility drops completion captures before the successor starts.                                                                                         |
| ImageRef computations and disk operations      | Generation-only `0cf65e6` production with identical worker gates fails the pending-computation assertion. Removing the new owner-completion disk check also fails both post-commit rejection/alias assertions. | Both real-Hermes executables pass with the worker implementation; the module passes again after restoring the post-commit check.                                     |
| Decode after file fetch has finished           | Original `QtImageCache` with identical gate tests fails because the decoder completes while its worker pool is blocked.                                                                                        | Final native cache fixture passes the worker, shared-clear and teardown checks.                                                                                      |
| Expo view publication after memory clear       | Original view source with the new shared epoch fails both fetch-pending and decode-pending cache assertions.                                                                                                   | Both cases display the requested pixels without cache repopulation; a fresh reload populates the cache. The full view suite passes 33 entries.                       |
| ImageRef alias publication after memory clear  | The original worker publisher from `92dea15`, with the new shared epoch and identical test, fails the cleared-memory alias assertion.                                                                          | The epoch-guarded publisher passes both real-Hermes executables; disk writing and readback still succeed after memory-only clearing.                                 |
| Disk clear and path lookup through JavaScript  | Original synchronous methods with the final worker/epoch dependencies fail the deferred filesystem assertion.                                                                                                  | Both real-Hermes executables pass; the new fixture verifies immediate invalidation, deferred sweep/path settlement, owner-loop progress and a surviving newer write. |

The memory-clear ImageRef regression additionally requires a successful disk
alias write after clearing memory, an empty `queryCache` result for that alias,
and successful decoding from disk. The post-commit disk-clear regression waits
for the writer without pumping owner events, admits a clear, and then checks
that the old completion rejects before publishing an alias. The first stripped-guard
mutation attempt was rejected by the unchanged `-Werror` flags for an unused
capture; the runnable control explicitly keeps that capture used and removes
only the guard behavior. It fails the two expected assertions, and the restored
source passes.

The cache-control fixture holds the native worker pool, admits path lookup,
clear and a new ImageRef write through the real Expo module, and checks that the
disk generation advances immediately while the old file and unresolved
Promises remain. An owner timer runs during the hold. After release, the path
lookup observes the clear and the newer write remains readable.

## Validation environment and commands

The host used Linux x86_64, Clang 19.1.7, Qt 6.11.1, CMake 3.31.10, Ninja
1.13.2, Node 26.13.1 and pnpm 12.18.0. Dependency pins and production CMake gates
were unchanged.

The full renderer was configured using the repository preset, with its existing
optional Expo Image and native UI flags enabled:

```sh
cmake --preset linux-host-renderer \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DRNQNX_ENABLE_QT_EXPO_IMAGE=ON \
  -DRNQNX_ENABLE_QT_NATIVE_UI=ON
cmake --build build/linux-host-renderer \
  --target rnqnx_expo_image_module_host_test rnqnx_expo_image_fabric_host_test \
  --parallel 3
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  build/linux-host-renderer/bin/rnqnx_expo_image_module_host_test -platform offscreen
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  build/linux-host-renderer/bin/rnqnx_expo_image_fabric_host_test -platform offscreen
```

These are ASan-enabled host executables. The preset does not add sanitizer
flags to every production runtime object; this receipt does not describe that
graph as fully instrumented.

The standalone cache and native UI graphs compile their production sources
with global ASan/UBSan flags:

```sh
cmake -S tests/image-loader -B build/image-loader -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/image-loader --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  build/image-loader/qnx_image_cache_test -platform offscreen

cmake -S tests/native/qt-native-ui -B build/native-memory -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/native-memory \
  --target rnqnx_image_units_test rnqnx_expo_image_item_test rnqnx_image_fetch_queue_test \
  --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  ctest --test-dir build/native-memory --output-on-failure --parallel 1 \
  -R '^rnqnx_(image_units|expo_image_item|image_fetch_queue)_test$'
```

The combined standalone native graph includes the reviewed fetcher follow-up
`26dcae8` and disk epoch core `4c917af`, together with the final worker helper,
rich cache and view memory generation. Its three affected CTests pass 90 QtTest
entries (44 image units, 33 view cases and 13 admission queue cases), with zero
failures or skips in 12.34 seconds. This graph covers the native cache and view
publishers; the ImageRef publisher is covered separately by the Hermes fixture.
The async loader wiring also passes syntax compilation with the exact primary
compiler, defines and include paths and the isolated candidate headers; that
compile check is separate from its functional JavaScript execution.

The seven existing router suites also passed in one ordered Node process:
the Expo wrapper, badge palette, drawer settlement, keyboard space, native
drawer, rendering and tab options. The keyboard case includes the actual
BottomTabView context, JavaScript/native bar paths and cleanup. These checks do
not establish the separately documented full prop, RTL and resize conformance.

## Build bookkeeping incident

An operator error briefly started two Ninja processes for the same primary
build directory during a build-job-count transition. Both processes were
interrupted before a single-process recovery. The overlap left an invalid
path-record checksum in `.ninja_deps`.

Two later sequential builds completed, but the next invocation rebuilt again.
Their results are recorded as actual rebuilds, not no-op verification. The
Ninja 1.13.2 recovery loader advanced its truncation position before validating
that malformed checksum, leaving the offending record in the recovered prefix.
Each subsequent load then discarded the valid records appended after it.

The malformed record started at byte 663504 and claimed path ID 2323 after
2328 valid paths. A backup, hashes and inode/size/timestamp records were saved.
With no visible Ninja/CMake process remaining, the documented Ninja
`-t recompact` operation rewrote the valid prefix. A read-only parser then
validated all 2328 path records and 289 dependency records across 663504 bytes.
No dependency record was invented and no build or test gate was bypassed.

After recompiling the missing dependency records, a complete parser validated
6041 path records and 854 dependency records across 2407868 bytes. The immediate
sequential CMake build reported `ninja: no work to do.` without a recovery
warning. The following worker candidate changed only seven build steps.

Source replacements in the controls use new writes rather than preserved file
timestamps. An initial standalone snapshot copy that preserved older timestamps
produced a no-op; its inputs were touched and actually rebuilt before any
candidate execution. Immutable copies of the first completed generation
candidate executables were tested while the unchanged-source recovery ran;
the copy hashes were checked before and after each snapshot.

## Final SVG stack integration

Published SVG prerequisite `d3eacc75dcad761c992736223a68b53ff852203b` is merged
in local source commit `3e381a30dc20afdee774fe26c12204040f71706e`. The only merge
conflicts were documentation: both historical handoff continuations were kept,
and the SVG guide combines uncached-pixel delivery with the accurate optional
QtSvg/plugin staging description.

The affected Expo module executable passes after this merge. The original
Fabric fixture timed out waiting for a layout event because its direct
`fabric.createNode` call recorded events without setting `onLayout: true`.
Pinned React Native `BaseViewProps` defaults that subscription flag to false,
and the generated ExpoImage props correctly delegate to `ViewProps`.
The fixture now requests the flag only for its existing layout scenario
(node 701). All initial/resized geometry and event assertions, production
guards and timeouts remain unchanged. The coordinator independently reviewed
that exact fixture correction; the corrected Fabric executable passes.

The [initial failed run](../evidence/pr159-image-workers-2026-11-17/final-stack-unsubscribed-fixture.json)
and its [unsubscribed fixture log](../evidence/pr159-image-workers-2026-11-17/rnqnx_expo_image_fabric_host_test-unsubscribed-fixture.log)
are retained. The [final identity record](../evidence/pr159-image-workers-2026-11-17/final-stack-validation.json)
records both passing executable hashes and the corrected fixture hash. The
[module log](../evidence/pr159-image-workers-2026-11-17/rnqnx_expo_image_module_host_test-final-stack.log)
and [corrected Fabric log](../evidence/pr159-image-workers-2026-11-17/rnqnx_expo_image_fabric_host_test-final-stack.log)
record the final passes. No full corpus replay or new target qualification is
claimed for this integration check.

## Architecture, review and rollback

This retains the existing core `ImageLoader` compatibility adapter and public
JavaScript API. Fabric-only rendering, Hermes, Qt, dependency pins and native
module architecture are unchanged. No package or license material was added.
Automatic header/cache isolation and TLS/redirect policy are preserved; the
loopback fixtures use synthetic data.

| Applied guidance                                          | Pinned source                                                              | Decision in this slice                                                                                                      |
| --------------------------------------------------------- | -------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| Repository `qnx-native-api` and `qnx-platform-validation` | PR baseline `e032a5b`                                                      | Keep the existing adapter boundary, validate real native behavior and retain target gates.                                  |
| Margelo `api-design` → `build-nitro-modules` → `cpp`      | `margelo/react-native-skills@1e9e17be6f41c838db472d6a4b943fc26355a3be`     | Preserve the public API and core-module exception; carry native values across workers and retain owner-thread JSI lifetime. |
| Callstack `react-native-best-practices`                   | `callstackincubator/agent-skills@59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Move computation/I/O off the event loop and test actual admission and completion ordering.                                  |

DRI: Expo Image completion agent (`finish_expo159`). Independent agent reviewers:
coordinator (`root`) and SVG/native contributor (`finish_svg161`), covering source
slices and regression design. Those agent reviews do not replace the pending
external target evidence requests.

The inherited six-task admission bound, 16 MiB encoded limit and 16-million-pixel
decode limit remain in place. Tests measure physical concurrency under teardown
and demonstrate owner timer progress behind a blocked worker. They do not
measure a new device frame-time or memory budget. QNX cross-compilation and a
Marathon/Wayland runtime remain distinct follow-up gates; the Metro checks below
are bundling evidence only.

Rollback should revert consumers before their worker/helper dependencies: SVG
rich delivery, async clear/path wiring, shared cache epochs/decoding, then the
ImageRef worker conversion. The independent fetcher lifecycle and disk epoch
fixes can remain when their callers are compatible. Retain the receipts and
rerun the affected host checks after a rollback. The next validation work is the
integrated candidate's exact target build, plugin/attribution and release asset
artifacts, followed by the device receipt.

## Other repository gates

`pnpm typecheck` passes both TypeScript configurations. The full `pnpm test:js`
runner stops at the existing Marathon development-launcher fixture because its
real Unix-domain socket bind returns `EPERM` for the temporary Wayland fixture.
The unmodified test is not counted as passing, and the suite was not weakened or
reported as complete. Hosted CI must supply the full-suite result in a suitable
environment.

The five affected checks pass explicitly after that stop: Expo host polyfill,
Expo module registration (24 checks), public-entry Image contracts, package
exports, and real Metro bundles for both QNX (717578 bytes) and Marathon
(717619 bytes). These focused passes supplement the separate seven router
suites; they do not turn the blocked full runner into a pass.

The final `pnpm format:check` and `pnpm validate:docs` outputs are retained with
the source tree's evidence. The latter also checks installed dependency drift
against the repository's documented pins.

## Related reviewed slices and remaining gates

- [Fetcher admission and teardown](pr159-fetcher-admission-2026-11-16.md),
  followed by [callback lifetime](pr159-callback-lifetime-2026-11-16.md).
- [Disk clear epochs](pr159-disk-clear-epochs-2026-11-16.md), including actual
  FIFO-blocked I/O, delayed/reversed clear ordering and failed unlink recovery.
- [SVG image delivery](pr159-svg-image-delivery-2026-11-16.md), including a
  synchronous callback that destroys the provider before `load` returns.

Independent source review covered the generation fix, worker ownership and
admission, callback destruction order, shared-memory publishers and async
clear/path wiring. The native controls test their observable effects and
owner/worker boundaries.

The repository's target evidence gates remain in force. This work does not
resolve the QNX plugin/attribution audit, actual release animated assets or
exact-device review requests. It makes no new target performance, TLS/plugin,
animation-format or whole-renderer-corpus claim.
