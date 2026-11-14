# PR 159: image, router and consumer review follow-up

## Candidate and scope

The reviewed remote base is
`d862625e79d6b1dd8a589eaf767e3311757c449a` on
`feat/expo-image-painted-view`. This receipt accompanies the follow-up source
commit and includes the separately reviewed keyboard and modal corrections:
[keyboard receipt](pr159-keyboard-space-2026-11-14.md) and
[modal receipt](pr159-modal-focus-2026-11-14.md). The dependency lock is unchanged,
SHA-256 `edf0327a216c8963c0ffea2300c37f0b7e29d05510e7f3fb5a3a2a4ad9feea49`.

The work was performed in a throwaway clone on Linux 6.18.57 x86_64, with
Node 26.13.1, pnpm 12.18.0, GCC 14.2.0, CMake 3.31.10,
Ninja `1.13.2.git.kitware.jobserver-pipe-1` and the
official host Qt 6.11.1 `gcc_64` package. Qt Core, Gui, Network, Qml, Quick and
Test were used; GUI tests ran offscreen with the software backend. Installed
pins remain React Native 0.88.1, React 19.3.0, Expo 58.0.24, Expo Router 58.0.31,
Hermes source `hermes-v260318099.0.5`, and Nitro
`1.0.0-alpha.nightly.136.gdb81de61bd32`. No dependency or feature flag changed.

## Defects corrected

- The generated external project's QNX tarball now carries the package's
  `src`, `adaptive`, `qt` and `svg` directories. The package manifest already
  listed them; the CLI's separate copy list omitted them. The original head's
  external consumer failed to resolve `@rn-qnx/react-native-qnx/svg/resolver`.
- The standalone SVG HTTP test links the image fetcher, fetch queue, disk
  cache and decode sources needed by the current `QtImageCache`. The original
  renderer-ON CI failed at that target's link step after its main renderer
  tests had passed.
- The back drawer's scrim moves with the content so it does not cover the
  exposed drawer panel. Both drawer sides honor a short fling before applying
  `swipeMinDistance`, using the native drawer's 500 px/s threshold and
  PanResponder's equivalent 0.5 px/ms velocity. Mixed per-tab badge palettes
  select the JavaScript bar that can represent them.
- Memory cache entries retain MIME metadata and encoded animation bytes; a
  cache hit emits `loadStarted` before `loaded` and creates its own QMovie for
  playback. SVG consumers retain their static first-frame lookup. The cache
  cost includes both the decoded frame and encoded bytes, and clear/eviction
  removes the complete entry together. The animation regression deletes the
  source after memory-only prefetch and still observes changing pixels and
  correct pause/start behavior in the Expo view.
- A caller-supplied `cacheKey` no longer merges requests with different
  authenticated headers or source URIs. Header and URI values are length
  framed before hashing. Requests without headers retain explicit alias
  behavior.
- Disk clearing advances a shared generation under the cache mutex. An HTTP
  request captures that generation at admission and cannot restore an entry
  after a clear. Response bytes must decode successfully before a disk write.
- `imageLoaded` forwarding belongs to the host's installed binding, including
  before the first `ImageLoader` lookup. Repeated installation requests preserve
  the original binding, retained module cache and single observer; host teardown
  removes that observer. The retained `SourceCode`
  instance receives the current URL on subsequent evaluations.
- BlurHash encoding uses the absolute AC magnitude. The unmodified official
  C reference was independently compiled to generate 17 encoder vectors.
  The historical npm vectors remain in the fixture because its TypeScript
  encoder uses signed maxima and differs on three noise-image cases. The
  new four-pixel negative-gradient vector is `1qI=Mq00`.

The runtime staging instructions now include the qtimageformats attribution
audit and both animated gallery assets. The old Pi screenshot folder is
explicitly marked incomplete device-run evidence: its original notes do not
record the host, exact deployed revision/hash, full commands or exit statuses.
No missing run facts have been inferred from screenshots.

## Source discipline and review

DRI: `expo_pr159`, performing the Qt renderer, core image adapter and Expo
router review. `hardening_prs` owns the keyboard/modal slices. The parent
integrator reviews and publishes the complete candidate; no external
CodeRabbit review is claimed because the CLI is signed out in this session.

Read and applied:

| Source or skill                                                                                                                                       | Decision                                                                                                                                                                                                        |
| ----------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Repository `qnx-native-api` and `qnx-platform-validation` at the base                                                                                 | Cite installed native symbols, preserve owner-thread completion, exercise failure cases, and separate host from target evidence.                                                                                |
| Margelo `api-design`, `build-nitro-modules`, then `cpp`, pinned at `1e9e17be6f41c838db472d6a4b943fc26355a3be`                                         | Preserve existing public contracts, bound ownership and cache behavior, and keep framework-internal adapters distinct from application Nitro APIs. No new native module API or generated binding is introduced. |
| Callstack `react-native-best-practices`, pinned at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`                                                         | Reproduce the observed behavior through real callers and avoid performance claims from source inspection.                                                                                                       |
| Installed Expo Router 58.0.31 `BottomTabView` and height contexts; RN `Libraries/Interaction/PanResponder.js:360-367`; native `QtDrawer.qml::endDrag` | Match the existing height callback and velocity units instead of inventing navigation behavior.                                                                                                                 |
| Installed Qt 6.11.1 `qthreadpool.h`, `qpromise.h`, `qfuturewatcher.h`, `qobject.h`                                                                    | Record the worker dispatch and completion seams in the [ledger](../cited-seams.md).                                                                                                                             |
| Official `woltapp/blurhash` C encoder                                                                                                                 | Use independent outputs and retain exact source Git blob identities beside the vectors.                                                                                                                         |

These are source reads, not installations or vendor endorsements. The image
changes preserve the fetcher's value-only worker captures and context-bound
completion; no raw JSI value or QObject is moved to a worker by this patch.

## Executed host validation

Commands ran from the source clone, except the consumer bundle commands, which
ran from the fresh project outside it. Every passing command below exited 0.
Logs are retained in [host evidence](../evidence/pr159-review-host-2026-11-14/).

| Command or check                                                                                                                                                    | Result and scope                                                                                                                                                                                                                                                                                                                           |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `pnpm install --frozen-lockfile`                                                                                                                                    | Passed with the exact package manager and unchanged lockfile. `react-test-renderer` is already a declared exact dev dependency.                                                                                                                                                                                                            |
| `pnpm typecheck`, `pnpm validate:dependencies`, `pnpm validate:manifests`, `pnpm validate:architecture`, `pnpm codegen:check`                                       | Passed.                                                                                                                                                                                                                                                                                                                                    |
| `node --test tests/js/qnx-router-*.test.mjs tests/js/qnx-project-create.test.mjs`                                                                                   | Seven affected suites passed.                                                                                                                                                                                                                                                                                                              |
| Ordered shared-process imports of `expo-router-qnx` and all six `qnx-router-*` tests                                                                                | Seven suites passed in the repository runner's ordering, including keyboard and native-bar routing.                                                                                                                                                                                                                                        |
| Four original-source controls                                                                                                                                       | The new tests each fail on the original scrim, fling, badge-palette and tarball-copy implementations, then pass with the corrections.                                                                                                                                                                                                      |
| `cmake -S tests/native/qt-native-ui -B build/pr159-native-ui -G Ninja -DCMAKE_BUILD_TYPE=Release`; build with `--parallel 2`; offscreen `ctest --output-on-failure` | Strict warning build passed and six executables passed. They cover image units, view events and animation, fetch scheduling, hash references, drawer/tab UI and modal behavior. The final fresh-MOC run below additionally confirms the held HTTP response across cache clear. These are Qt host tests without Fabric or a QNX compositor. |
| `cmake -S tests/image-loader -B build/pr159-image-loader -G Ninja -DCMAKE_BUILD_TYPE=Release`; build and offscreen CTest                                            | Passed 1/1. Real loopback HTTP, metadata, animation, headers, failures, timeout, limits, cache reuse and teardown.                                                                                                                                                                                                                         |
| Fresh CLI project creation with `--with-marathon`, its own `pnpm install`, `pnpm run bundle:qnx`, `pnpm run bundle:marathon`                                        | Both actual Metro bundles passed; no workspace package links were substituted. QNX bundle SHA-256 `e14eec08251d1b408d576dd200d08dcc41e55bb6f552eed2720e558de095c075`; Marathon `906c8b3a19bb9532bf030194d97aff10fbe48f55650576580666160e04acf81d`.                                                                                         |

The full `pnpm test:js` runner was attempted and exited 1 at
`marathon-development-launcher.test.mjs:89`: this environment rejected the
Unix socket `listen` for its Wayland fixture with `EPERM`. No test or gate was
disabled. The affected router suites were additionally exercised in their
real shared-process order as recorded above.

The standalone SVG HTTP target now builds and passes all 15 QtTest entries
under Debug ASan/UBSan, with no skipped entries or sanitizer diagnostic:
`cmake -S tests/native/qt-rnsvg-image-http -B build/pr159-svg-http-asan -G Ninja
-DCMAKE_BUILD_TYPE=Debug -DRNQNX_QT_RNSVG_SANITIZERS=ON`, followed by the build
and CTest. Leak detection is disabled by the existing target; Qt libraries are
not instrumented, so this is the application/renderer scope only. Once the
original missing link sources were fixed, two old assertions were exposed:
local reads now finish on workers, and safe same-origin redirects are supported.
The tests now wait for the real asynchronous completion, prove safe redirects
load, and still reject a redirect to an unsafe scheme. No assertion was replaced
with an unconditional pass. A new animated prefetch case confirms SVG retains
its first-frame lookup while the Expo view can restore animation.
The new Hermes-host `imageLoadedForwardingDoesNotRequireLoaderLookup` and
`retainedSourceCodeTracksSubsequentEvaluations` regressions now pass in the
corrected full renderer target described below, with failing original-source
controls and a successful restored-candidate rerun. Fresh renderer ON/OFF
checks and exact-head hosted CI remain separately required.

## Remaining gates, security and rollback

No QNX cross-build, physical Pi, stock QSTI, QNX VM, actual Marathon shell or
performance run was performed for this source delta. Old device receipts
remain tied to their original source and cannot qualify this candidate.
Current-target input, image/cache/animation and teardown observations need a
bounded run with host, board/image, deployed hashes, full commands and exit
statuses. Existing wider explicit-cache/ImageRef worker-conversion work remains
outside this review slice. Required target gates stay open.

The authenticated cache-key separation removes an image disclosure path; cache
clearing no longer permits pre-clear fetches to repopulate disk state. No new
network endpoint, permission, telemetry, licensed payload or package version
was added. The corrected attribution instructions were inspected but no new
QNX runtime/attribution package was built or audited here.

Rollback is the follow-up commits in reverse order. No deployed device state,
release gate or CI requirement was modified. Next: inspect exact-head hosted
checks/reviews, and
run the remaining target qualification before claiming device completion.

## Main and SVG integration

After source commit `220b6e96857c6dcb1fdb5dd49ede861fb434e9a4`, merged
PR 161 integration commit `446d28ff47131a5032fb476987ada08c751cd697`, which
contains main `d08e2e9` and the nested SVG-root corrections. Two conflicts in
public Qt staging were resolved by preserving the transaction, rollback, lock
and path-boundary checks while retaining PR 159's five mandatory imageformat
plugins and multiple SPDX audit inputs. Imageformat copies are restricted to
the selected `IMAGEFORMATS_BUILD`; that tree is also protected from output
collisions. Both the attribution and recovery documentation are retained.

`node tests/js/qnx-public-runtime-staging.test.mjs` passed all 22 synthetic
host fixtures, including the original 17 checks plus combined base/imageformat
notices, missing plugin rollback, imageformat symlink escape, imageformat
output collision and malformed second-audit rollback. This proves the merge's
script behavior with fixture files and fake readelf, not a real Qt attribution
audit or target runtime package. The original-source host logs above remain
attached to their tested source; the full renderer and exact-head CI qualify
the integrated native candidate separately.

Subsequently merged PR 161 follow-up
`13a0e49ae7ea6b16faa23840926d3976866f6646` as local
`df981e53104fd32f8ba343211fa4a6d44511c006`. It adds the GCC kernel-size-before-move
fixture correction, its 8/8 standalone SVG sanitizer evidence, and the PR 161
handoff/readiness records. Both PRs' records were preserved; the generated
project state and documentation validation pass. It introduces no additional
production native change beyond the earlier `446d28f` integration.

The parent integrator's staging review then found that a custom
`IMAGEFORMATS_SRC` was honored by the builder and audit but ignored for notice
copies. The stager now reads only qtimageformats' module, third-party and
attributed license files from the selected extraction tree. Those copies are
bounded to that module root, and staging rejects outputs that contain or lie
inside it. The new custom-source fixture fails against the `df981e5` script:
its generated runtime has no `licenses/qtimageformats/MIT.txt`. The fixed
`python3 tests/python/test_qt_public_staging.py -v` passes all 25 fixtures,
including selected-source output collision and notice-symlink escape cases,
in 7.156 seconds. Raw negative and candidate logs are retained alongside the
earlier 22-case integration log. These synthetic notice checks do not provide
a current QNX runtime attribution audit or release-app/device evidence.

## Independent paused-animation follow-up

The independent `svg_pr161` review found one additional image defect: an
`autoplay=false` QMovie has no current image before its first start, so the
view ignored its already decoded first frame and painted transparent pixels.
`Layer::frame()` now falls back to that decoded image until QMovie has a frame;
it does not seek or decode another frame on the GUI thread. The reviewer found
no other actionable issue in the animation/MIME cache, bounded cost, request
identity, disk-generation or SVG first-frame changes.

The strengthened existing GIF regression first loads with autoplay disabled,
checks stable red pixels before start, observes blue after start, and checks
the retained pixels after stopping. It runs both a direct decode and a
memory-prefetched source whose original file is deleted. With production
source at `71ba54d`, both rows fail at the initial pixel assertion: actual
`#00000000`, expected `#ffff0000`; the focused command exits 2. With the fallback,
the focused run passes 4/4 QtTest entries in 878 ms. The strict Release rebuild
and all six native-UI CTests pass again on the merged candidate in 29.48 s,
with no failed or skipped QtTest entries. The focused command is
`QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
build/pr159-native-ui/rnqnx_expo_image_item_test
animatedGifPlaysAndAutoplayFalsePauses`; the complete run uses
`ctest --test-dir build/pr159-native-ui --output-on-failure` with the same
environment. Raw negative, build, focused and complete outputs are in the
host evidence directory. At that stage the full Hermes run was pending; its
completed candidate/control/restoration cycle is recorded below. Hosted CI
and target gates remain open.

## Image validation source identity

The image-validation commit is
`252b128d0bbb93357dd87f840ea81d31a08c5ef1`, with Git tree
`3a19dc66775917c333a61d219400cd73ecb8bc5b`. Its first evidence consolidation,
local `f2db6ea0ee8740fdce56f6c3c17f82774e6c0f75`, was published as
`f49e61eeadaf72ebc4b44060f94d1b97d75418f9`; both have tree
`df9170b388952c2f00cac9063885a872277122b2`. Connector publication can produce
another commit identity; the original image-validation source is identified
independently in [tested-source.json](../evidence/pr159-review-host-2026-11-14/tested-source.json).
It records complete Git subtree identities for `apps`, `native`, `packages`,
`scripts`, `tests` and `vnext`, plus SHA-256 and byte counts for 55 changed source,
test, build and dependency-instruction files. These identities were read from
the validation commit and checked against the frozen checkout, allowing the
published source to be compared without relying on its commit SHA alone.
The later `QtReactHost.cpp` corrections and their full-host source inventory
are recorded separately below. The 126-entry Release and 73-entry image
sanitizer results retain their original `252b128` scope.

## Final image sanitizer validation

After independently reviewing the final paused-frame fallback, `svg_pr161`
configured a fresh Debug build of the affected image graph at the frozen
validation source. GCC 14.2 instruments the image object library and three
test executables with `-fsanitize=address,undefined`,
`-fno-sanitize-recover=undefined` and `-fno-omit-frame-pointer`; the linker uses
`-fsanitize=address,undefined`. Compile and CMake autogen concurrency are both
two. The exact Qt 6.11.1 shared release libraries remain uninstrumented.

Configuration and both builds exited 0. With
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1`, the software/offscreen backend and the
unchanged 90-second CTest timeout, the executed targets passed:

| Target                         |                 QtTest entries | CTest duration |
| ------------------------------ | -----------------------------: | -------------: |
| `rnqnx_expo_image_item_test`   | 31 passed, zero failed/skipped |         9.10 s |
| `rnqnx_image_units_test`       | 29 passed, zero failed/skipped |         1.85 s |
| `rnqnx_image_fetch_queue_test` | 13 passed, zero failed/skipped |         1.56 s |

All three CTests exited 0: 73 QtTest entries and no sanitizer diagnostic.
The logs explicitly include both initially paused GIF data rows and
`clearingDiskCacheDoesNotRestoreAnInFlightResponse`. This is host evidence
for the instrumented project graph, with leak checking disabled; it does not
exercise Hermes/Fabric, QNX, a physical device or Marathon. The separate full
Hermes run is recorded independently. Raw configure/build/verbose CTest logs,
actual generated compiler flags, source identity and 13 checked source hashes
are retained under [image-sanitizers](../evidence/pr159-review-host-2026-11-14/image-sanitizers/).

Comparing these logs exposed a coverage gap in the earlier incremental Release
run: its generated `ImageFetchQueueTest.moc` registered 12 entries and omitted
the new disk-clear HTTP regression, while the fresh sanitizer build registered
and passed all 13. The prior 125-entry Release logs remain intact and do not
qualify that omitted case. With the source unchanged, removed only that target's
generated MOC and autogen timestamp, rebuilt the fetch-queue executable, then
reran all six Release CTests. All passed in 29.75 seconds, now with **126 QtTest
entries**, zero failures/skips, and an explicit pass for
`clearingDiskCacheDoesNotRestoreAnInFlightResponse`. The exact refresh commands,
build output and complete test results are preserved under
[release-moc-refresh](../evidence/pr159-review-host-2026-11-14/release-moc-refresh/).

## Initial full Hermes host attempts

`hardening_prs` configured the full renderer host graph against the frozen
source in `build/host-review159`, using the exact pinned Hermes source
`9a115f15785851f4ff2747ebb090ddd066fc8ad2`, Qt 6.11.1, GCC 14.2 and ICU 76.1
headers/libraries. The existing input/Qt feature switches remain enabled;
compile/autogen concurrency is two. The pinned Hermes/JSI dependency build
and the actual playground Metro bundle with two workers completed
successfully.

The subsequent `qt_react_host_host_test` build failed at step 137 of 550,
before linking or executing tests. Pinned React Native's generated
`ReactNativeFeatureFlags.cpp:25` unconditionally uses
`#pragma GCC diagnostic ignored "-Wglobal-constructors"`. GCC rejects this
Clang-specific warning name under the existing `-Werror=pragmas`. The failed
command and diagnostic are preserved in
[gcc-qt-react-host-build.log](../evidence/pr159-review-host-2026-11-14/full-host/gcc-qt-react-host-build.log),
alongside configuration, dependency-build, Metro and environment records.
No upstream source or warning policy was changed to pass this build.

A supported Clang 19.1.7 retry used the separate
`build/host-review159-clang` directory. It reused verified dependency source
trees and the actual Metro bundle while rebuilding compiler-dependent objects.
The initial parallel-two build was intentionally interrupted with exit 130
after other heavy builds finished, then resumed at parallel four; autogen
remained two. That resource adjustment, preserved in `build.log` and
`resume.json`, is distinct from the following actual failures.

Clang exposed an invalid heterogeneous lookup at `QtReactHost.cpp:716`:
`QnxSourceCode::kModuleName` is a string view but the map has string keys and
a nontransparent hash. Commit `70c170c1bed83c3cbbb8ae6f5bcae48cacea218e`
adds the explicit `std::string` conversion. This candidate linked, but its
first execution failed during the unchanged image-event regression's repeated
binding installation: pinned RN defines `nativeModuleProxy` as a read-only
global, so a second installation throws. Thirteen cases had passed before
exception teardown aborted in Hermes; this is retained as a failed CTest run,
not a completed host result.

Commit `b6bf7eb6b37827a592a4547fb4ac7137557f463f` adds a per-host installed
flag, checked before invoker/observer setup and set only after upstream
installation succeeds. Repeated requests preserve the binding, retained
modules, extra-provider behavior and single observer. Independent review
confirmed that each Impl creates one runtime with no replacement path, the
flag resets with a new host, and the existing constructing-thread model is
unchanged. The regression tests were not edited. Both failed attempts and the
incremental correction builds are preserved in
[full-host-clang](../evidence/pr159-review-host-2026-11-14/full-host-clang/).

## Corrected full Hermes host validation

The tested source is `b6bf7eb6b37827a592a4547fb4ac7137557f463f`, tree
`d2f56c2d54de785744c76d8f1a69a87e14d53edf`. The toolchain is Clang 19.1.7,
Qt 6.11.1, Node 26.13.1, pnpm 12.18.0, CMake 3.31.10 and Ninja 1.13.2.
Exact compiler packages, source hashes, configured features and flags are
retained with the logs. The existing host test target is ASan-instrumented
and linked; its linked libraries are not all instrumented, and this target
does not enable UBSan. Leak detection remains disabled by its existing CMake
configuration. This run executes only `qt_react_host_host_test`, with all 59
enabled cases and no skips; it does not claim the complete CTest matrix.

| Phase                             | Result                                                                                                 | Duration |
| --------------------------------- | ------------------------------------------------------------------------------------------------------ | -------: |
| Corrected candidate               | CTest 1/1, exit 0; all 59 host cases passed                                                            |  76.24 s |
| Five-file original-source control | CTest exit 8; 57 passed and exactly the two intended regressions failed, with normal runner completion |  75.07 s |
| Restored candidate                | CTest 1/1, exit 0; all 59 host cases passed again                                                      |  69.28 s |

The control used the candidate checkout and unchanged regression test file,
replacing only `QtReactHost.cpp`, `QnxImageLoader.{h,cpp}` and
`QnxExceptionsManager.{h,cpp}` with their `d862625` versions. It is a controlled
five-file hybrid, not a full old checkout. Only
`retainedSourceCodeTracksSubsequentEvaluations` and
`imageLoadedForwardingDoesNotRequireLoaderLookup` failed; the retained-module
lifetime case still passed. All five source files were restored in a finally
path, rebuilt and retested. Their hashes match the candidate, and the unchanged
test SHA-256 is `1c52a62c0243445f553bc216c63636b48f5a9639974489e54adbbea2f8462f18`.

The command was `ctest --test-dir build/host-review159-clang -R
'^qt_react_host_host_test$' --no-tests=error --timeout 180
--output-on-failure -V`, with offscreen/software rendering and the target's
existing sanitizer environment. Incremental baseline/restoration builds used
`cmake --build build/host-review159-clang --target qt_react_host_host_test
--parallel 4`. The full commands, outcomes and case lists are in
[summary.json](../evidence/pr159-review-host-2026-11-14/full-host-clang/regression-controls-idempotent/summary.json).

The initial and restored candidate executables are byte-identical:

- `qt_react_host_host_test`: 146,402,544 bytes; SHA-256
  `55af47f994614c2ca0110e3dd77004822b0c479772631c7afe4fbb2355712cd1`.
- The actual Metro bundle: 784,360 bytes; SHA-256
  `761ef40b9100ae5eec2a9ee830e4a24a33d45554bf7a9784673c27cc5b6ea817`.

The receipt owner independently rechecked the restored production/test hashes
and both artifact hashes. Source-tree identities and 55 file hashes for this
corrected host candidate are in
[full-host-tested-source.json](../evidence/pr159-review-host-2026-11-14/full-host-tested-source.json).

After the controlled cycle released the clone, merged standalone PR 161's
consumer follow-up `3b24f5c2fd3733dbf1ebe77729f7014242db7db3` as local
`d92312784c80e79d1e7df8aa2e29dd51b2a99b10`. Its two CLI/test blobs are exactly
the packaging fix already present in PR 159, independently reviewed in both
branches. The merge preserves every tested source-tree identity while retaining
both PRs' receipts and handoff/readiness records. All 11 release gates remain
pending. The two host findings now have executed candidate/control/restoration
evidence; exact-head hosted CI, actual codec attribution output, release-app
animation and current-device receipts remain required. No QNX or physical-device
result is inferred from these host runs.
