# PR 159: disk-cache clear admission and ordered sweeps

Date: 2026-11-16 America/New_York (execution continued into November 17 UTC).
Base: `0cf65e6bcb5462d14b7cafa3e2a9c360ca6168ef`.

The disk cache now separates immediate invalidation from filesystem removal.
`beginClear()` advances the shared generation without taking the disk I/O
mutex. `completeClear(epoch)` performs the sweep on a worker and reports
success. A read or write that reaches the disk first applies a pending sweep;
a delayed clear worker then preserves entries written in the new generation.
Failed deletion leaves the sweep pending, so old files cannot become cache hits
and new writes cannot report success until removal succeeds.

This receipt qualifies the low-level native cache only. The separate PR 159
loader work connects these methods to bounded asynchronous operations and
checks the generation again on owner completion before publishing decoded
aliases. This slice does not itself establish that JavaScript APIs run off the
owner thread. Exact combined-head CI and current QNX target evidence remain
required; all existing release gates remain pending.

## Architecture and changed paths

| Path                                                          | Change                                                                                                                                             |
| ------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `vnext/include/react-native-qnx/views/image/ImageDiskCache.h` | Add `beginClear()` and `completeClear(epoch)`; retain `void clear()` as a synchronous compatibility helper.                                        |
| `vnext/src/views/qt/image/ImageDiskCache.cpp`                 | Share an atomic requested generation and a mutex-protected completed-sweep generation across cache copies; order reads, writes, and clear workers. |
| `tests/native/qt-native-ui/ImageUnitsTest.cpp`                | Add six behavior cases covering real blocked I/O, pending reads, delayed and overlapping clear workers, and deletion failure/retry.                |

The on-disk SHA-256 filename format, byte budget, LRU policy, default feature
flags, renderer, package pins, and dependencies are unchanged. `usedBytes()`
continues to report physical disk usage synchronously, including entries that
a failed clear has hidden. Independently constructed cache objects still do
not share a lock; callers must copy the shared cache state for worker lifetime.

The write path checks its admitted generation before the sweep, before commit,
after commit, and after eviction. If a clear arrives during filesystem work,
the write returns false and removes its obsolete committed file when possible.
The pending sweep continues to hide any file whose removal fails. Because a
clear can arrive after a worker returns, loader code must also validate the
generation when its owner callback runs.

The sweep updates its completed generation only after every matching cache
file is removed. Directory access and unlink failures return false. Repeating
an already completed clear is a no-op, including when clear workers complete
out of admission order. The existing synchronous helper admits and completes
the same operation while retaining its original void return type.

## Source inspection, skills, and review

The startup contract, current PR 159 handoff, component table, Qt Quick ADR,
open-source Qt ADR, native API policy, and image-cache/fetcher source were read.
The native API and platform validation repository skills match the previously
read sources at the PR 161 baseline; changed PR 159 handoff/support documents
were read separately. No additional skill installation is claimed.

| Skill/source                                              | Exact version                              | Decision applied                                                                                                                           |
| --------------------------------------------------------- | ------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------ |
| Repository `qnx-native-api` and `qnx-platform-validation` | Base `0cf65e6`                             | Preserve the existing C++ boundary and shared native lifetime; distinguish standalone host behavior from Hermes, QNX, and device evidence. |
| Margelo `api-design` → `build-nitro-modules` → `cpp`      | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Keep admission separate from blocking work and document the failure contract; no Nitro API or ABI change is introduced.                    |
| Callstack `react-native-best-practices`                   | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Keep slow work away from owner admission and test the ordering boundary rather than infer it from a Promise return value.                  |

The image-cache DRI was `/root/finish_svg161`. Independent source/test reviews
by `/root/finish_expo159` and `/root` found no blocking issues. The final source
blobs and SHA-256 values are recorded in
[source-validation.json](../evidence/pr159-disk-clear-epochs-2026-11-16/source-validation.json).

## Native execution and negative controls

The host was Debian 13 x86-64, UID 1000, using Clang 19.1.7, Qt 6.11.1,
CMake 3.31.10, and Ninja 1.13.2. The standalone image graph was built as Debug
C++20 with `-Wall -Wextra -Wpedantic -Werror`, AddressSanitizer,
UndefinedBehaviorSanitizer, frame pointers, and undefined-behavior recovery
disabled. Qt used its offscreen software backend. The supplied Qt shared
libraries were not rebuilt with sanitizers. Leak detection was disabled;
`UBSAN_OPTIONS=halt_on_error=1` was set.

| Execution                                       | Result                                                      | Evidence                                                                                      |
| ----------------------------------------------- | ----------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| Six new behavior cases                          | 8 QtTest entries passed, 0 failed/skipped; 84 ms            | [Focused candidate log](../evidence/pr159-disk-clear-epochs-2026-11-16/candidate-focused.log) |
| Same cases with four targeted control mutations | All six behavior cases failed as intended; exit 6; 2,068 ms | [Mutation log](../evidence/pr159-disk-clear-epochs-2026-11-16/mutation-negative.log)          |
| Three affected native CTests                    | 3/3 passed; 79 QtTest entries; 0 failures/skips; 11.62 s    | [Verbose CTest log](../evidence/pr159-disk-clear-epochs-2026-11-16/candidate-ctest.log)       |

QtTest entry totals include `initTestCase` and `cleanupTestCase`. The full run
contains 35 image-unit entries, 31 image-item entries, and 13 queue entries.
The deletion failure was exercised as an unprivileged process by removing
write permission from the cache directory while retaining an existing file.
The test restores permissions before removing its temporary directory. The
FIFO admission case keeps a real read blocked under the disk mutex while a
second worker calls `beginClear()` and `generation()`; both return before the
read is released.

An initial FIFO-test prototype joined the reader while its writer remained
open and timed out. The corrected guard closes the writer before joining the
workers. The passing focused and complete logs above use that corrected test.

The new split API cannot compile against the original implementation, so the
negative run is explicitly a mutation control, not an original-source run.
The controls add the disk lock to admission, omit the read/contains sweep,
force a completed clear to sweep again, and accept failed unlink operations.
Their failures respectively demonstrate blocking admission, stale physical
entries, deletion of newer writes, and an incorrectly advanced sweep marker.
The exact delta is in
[mutation-negative-controls.patch](../evidence/pr159-disk-clear-epochs-2026-11-16/mutation-negative-controls.patch).

Only a separate mutated `ImageDiskCache` object was substituted into the
control executable. The candidate source, candidate object, and compiled
test object were untouched. Compiler/link/run arguments and before/after
source/test hashes are in
[mutation-commands.json](../evidence/pr159-disk-clear-epochs-2026-11-16/mutation-commands.json).
The complete CTest run used the unchanged candidate executables.

### Commands

After selecting the stated Clang/Qt toolchain:

```sh
cmake -S tests/native/qt-native-ui -B build/disk-epoch-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build/disk-epoch-asan --target \
  rnqnx_image_units_test rnqnx_expo_image_item_test rnqnx_image_fetch_queue_test -j2
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  build/disk-epoch-asan/rnqnx_image_units_test -platform offscreen \
  diskCacheClearAdmissionDoesNotWaitForIo \
  diskCachePendingClearIsAppliedByReaders \
  diskCacheDelayedClearPreservesNewWrites \
  diskCacheOverlappingClearsPreserveNewestWrites \
  diskCacheFailedClearStaysPending
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/disk-epoch-asan -V \
  -R '^rnqnx_(image_units|expo_image_item|image_fetch_queue)_test$'
```

Full pinned formatting and documentation validation also passed (24 components,
11 unchanged release gates); their logs are preserved beside the native evidence.

Configuration and compilation logs are preserved beside the test evidence.
Optional Qt TaskTree/Vulkan-header discovery warnings did not prevent the
standalone graph from configuring and compiling.

## Remaining validation and rollback

This is Linux host evidence without Hermes, Fabric, Wayland, QNX cross-build,
or device execution. The Unix FIFO case and Unix directory-permission failure
case explicitly skip on unsupported hosts; the latter also skips as root.
Neither skipped here. Configure-cache diagnostics remain synchronous in the
separate loader design. No latency, throughput, target I/O, or memory budget
measurement is claimed by these functional tests.

The next PR 159 integration must run the real-Hermes worker and owner-callback
controls with this core, the shared decoded-memory epoch, and the fetcher
lifetime correction, followed by exact-head CI and fresh combined QNX
qualification. There is no new licensing, packaging, network permission, or
dependency requirement in this slice. No target system configuration changed.

Rollback restores the three implementation/test paths to the base above and
must be coordinated with any later loader code calling the new methods. The
cache file format needs no migration. Retain this dated evidence and the
existing draft/target gates when integrating or reverting the change.
