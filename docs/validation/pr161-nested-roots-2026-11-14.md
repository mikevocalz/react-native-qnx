# PR 161 nested SVG continuation

Base: `d38097f38d5a1ebb560627649e95c84d04d32f02` on
`feat/rnsvg-close-gaps`. The dependency lock SHA-256 is
`4198fb84cd0abd9e8da568af4741a12c2318280827d3b823727e92ed98b2b565`.

## Behavior and source boundaries

The earlier outward invalidation fix covers prop changes. A direct child
removed from a nested root still called `update()` on the hidden root. This
continuation sends child additions and removals through the same outward
invalidation path. The real-window regression removes a painted child and
requires the visible root to repaint its former pixels white.

`Bounds::nestedBox` previously skipped a root directly inside another nested
root. An enclosing group with opacity could therefore allocate no paint layer
for that content. Bounds now traverse both element and root children. Each
nested viewBox transform and viewport clip applies at its own level, with the
existing recursion limit. The regression checks the transformed bounds,
translucent pixels and clipped pixels through two nested viewBoxes.

The native behavior changes are confined to `native/qt/rnsvg/RnsvgItems.cpp`
and `native/qt/rnsvg/scene/Bounds.cpp`, with regressions in the existing scene
test. Fabric, Hermes and Qt Quick remain the backend; the SVG option remains
default OFF. There is no public API, dependency, target-toolchain or scheduling
change.

The merge of main `d08e2e9b61ed78d9f5fbbc522a4423e0a38d98c4` is published as
`446d28ff47131a5032fb476987ada08c751cd697`. It retains the transactional
staging from PR 163 and integrates the optional QtSvg artifact copies inside
that transaction. Only the two explicit SVG copy calls accept `SVG_BUILD` as
a source root. Other copies retain the existing stage/source boundaries. The
SVG input tree also participates in the output ancestor/descendant guard.
Locking, recovery journals, candidate validation, rollback and final cleanup
remain intact. Four added fixtures cover the SVG library/plugin and dependency
closure, rollback on a missing plugin, a plugin symlink escape, and output
paths that would overlap the SVG input tree.

The GCC native run also exposed an existing test construction bug:
`detail::convolve(f, k.size(), 1, std::move(k), ...)` permits the vector to be
moved before its size is read. Because the kernel parameter is passed by
value, that ordering constructs an invalid zero-order filter. The fixture
now reads the size before the move. Every pixel assertion, expected value,
timeout and tolerance remains unchanged; no production filter code changes.

## Validation

Local environment: Debian 13 x86-64, Node 26.13.1, pnpm 12.18.0,
GCC 14.2.0, CMake 3.31.10, Ninja 1.13 and Qt 6.11.1.
Selected command output is in [local-checks.txt](../evidence/pr161-nested-roots-2026-11-14/local-checks.txt).

| Command or check                                    | Result  | Scope                                                                                                      |
| --------------------------------------------------- | ------- | ---------------------------------------------------------------------------------------------------------- |
| `pnpm install --frozen-lockfile`                    | passed  | Exact 522-package graph; no lockfile edit.                                                                 |
| `pnpm validate:dependencies`                        | passed  | Manifest pins and installed source contracts.                                                              |
| `pnpm typecheck`                                    | passed  | Root and adaptive TypeScript projects.                                                                     |
| `node tests/js/rnsvg-corpus-packing.test.mjs`       | passed  | All 149 fixtures, 16 batches, four nonempty shards.                                                        |
| `node tests/js/rnsvg-overlay-render.test.mjs`       | passed  | Actual Metro render: 57 QNX probes and 57 unchanged iOS probes.                                            |
| `git diff --check`                                  | passed  | Changed source and tests.                                                                                  |
| Native scene baseline negative controls             | passed  | Both regressions fail on the original two native source files, exit 2.                                     |
| Native scene candidate regressions                  | passed  | Both pass on the restored `3324e7c` native source, exit 0.                                                 |
| Convolution fixture after size-before-move fix      | passed  | Same pixel assertions and tolerances; focused test passes in 5 ms.                                         |
| Standalone SVG ASan/UBSan suite                     | passed  | All eight CTests pass in 34.86 s after the test construction fix.                                          |
| Python transactional staging fixtures               | passed  | All 17 pass, including the four added SVG integration cases.                                               |
| Target doctor observation fixtures                  | passed  | 47 scenarios plus file/CLI boundaries; host fixtures only.                                                 |
| Typecheck and docs after main integration           | passed  | Root/adaptive types; 24 components and 11 release-gate documents.                                          |
| Project-create archive regression, original source  | passed  | Expected exit 1: generated package lacks `package/svg/resolver.js`.                                        |
| Project-create archive regression, corrected source | passed  | Actual package contains SVG, Qt and adaptive public entry files; existing creation/refusal checks pass.    |
| Fresh dual consumer install and Metro bundles       | passed  | 445 packages installed; QNX and Marathon production bundles each exit 0 with two workers.                  |
| Hosted workflows at `13a0e49`                       | passed  | All nine workflows; ON full graph 49/49, all four corpus shards, SVG sanitizer 8/8 and HTTP sanitizer 1/1. |
| QNX cross-build and physical-device execution       | not run | No licensed SDP or connected target in this Cloud environment.                                             |
| Marathon/Wayland execution                          | not run | No target-specific implementation change; no target evidence promoted.                                     |

### Standalone consumer packaging continuation

The published native head `13a0e49ae7ea6b16faa23840926d3976866f6646`
still staged only `index.js`, `Libraries` and `expo` for the locally packed
`@rn-qnx/react-native-qnx` package. Its Metro configuration requires
`@rn-qnx/react-native-qnx/svg/resolver`, so a new external consumer could not
load that configuration. The package manifest already included the intended
source and public entry directories; the CLI's separate staging list omitted
them.

This follow-up ports only the reviewed copy-list correction and archive
regression from PR 159 commit
`f49e61eeadaf72ebc4b44060f94d1b97d75418f9`. The CLI now copies `src`,
`adaptive`, `qt` and `svg` through the existing bounded-copy helper. The test
inspects the actual generated archive and requires the SVG resolver plus
the Qt and adaptive entry files. The two complete file blobs match the
published PR 159 files exactly:

| File                                                   | Git blob                                   |
| ------------------------------------------------------ | ------------------------------------------ |
| `packages/react-native-qnx-cli/src/create-project.mjs` | `cd36b50135a005915f4d1d68f53da82e2c19da33` |
| `tests/js/qnx-project-create.test.mjs`                 | `88ce636f364330a1e30a0b7eb0bae5d51eb8f539` |

With the new test and original CLI source, `node
tests/js/qnx-project-create.test.mjs` exits 1 at the missing
`package/svg/resolver.js` assertion. After the copy-list correction the same
test passes, including its existing real-archive, hash, identity, refusal and
no-overwrite checks. The independent Expo integration reviewer compared only
these two files with the published source and found no blocker; bounded
copying, manifest/version pins and source behavior are preserved.

A new directory outside the checkout was created with the corrected CLI:

```sh
node packages/react-native-qnx-cli/src/bin.mjs project create \
  --project /workspace/scratch/853552216e74/pr161-created-dual \
  --app-id org.example.svgconsumer --name 'SVG Consumer' --with-marathon
cd /workspace/scratch/853552216e74/pr161-created-dual
pnpm install
pnpm exec metro build index.js --config metro.config.js --platform qnx \
  --dev false --max-workers 2 --out build/qnx/app.bundle.js
pnpm exec metro build index.js --config metro.marathon.config.js --platform marathon \
  --dev false --max-workers 2 --out build/marathon/app.bundle.js
```

Creation and install succeed, with 445 consumer packages installed using
Node 26.13.1 and pnpm 12.18.0. Both Metro commands exit 0. The QNX bundle is
716,848 bytes; the Marathon bundle is 716,874 bytes. All four local archive
hashes match `platform-packages.json`, and the platform archive contains all
three asserted entries plus `src`. This uses the generated dual-target
public-import starter. It does not claim the Expo Router starter ran, that
either bundle executed in a native host, or that a clean licensed developer
workflow is qualified. The repository dependency lock remains unchanged.
No native source or native test is changed or rerun for this packaging slice.

| Evidence                                                                                                                                                                                                           | Observation                                                                     |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------- |
| [Original CLI with new archive assertion](../evidence/pr161-nested-roots-2026-11-14/consumer-negative-project-create.log)                                                                                          | Expected missing-resolver assertion, exit 1.                                    |
| [Corrected project-create regression](../evidence/pr161-nested-roots-2026-11-14/consumer-project-create.log)                                                                                                       | Real archive and existing creation/refusal checks pass.                         |
| [Create](../evidence/pr161-nested-roots-2026-11-14/consumer-create.log), [install](../evidence/pr161-nested-roots-2026-11-14/consumer-install.log)                                                                 | New external dual consumer with four local platform archives; install succeeds. |
| [QNX bundle](../evidence/pr161-nested-roots-2026-11-14/consumer-qnx-bundle.log), [Marathon bundle](../evidence/pr161-nested-roots-2026-11-14/consumer-marathon-bundle.log)                                         | Both production Metro bundles succeed with two workers.                         |
| [Source/archive/bundle inventory](../evidence/pr161-nested-roots-2026-11-14/consumer-validation.json), [platform archive listing](../evidence/pr161-nested-roots-2026-11-14/consumer-platform-archive-listing.txt) | Exact source blobs, SHA-256 hashes, commands, exit codes and output sizes.      |

Follow-up [typecheck, documentation and formatting output](../evidence/pr161-nested-roots-2026-11-14/consumer-checks.txt)
records successful checks with the repository lock and all 11 release gates
unchanged. Consumer Metro logs have trailing whitespace normalized; originals remain in
the validation workspace. The packaging follow-up still needs its own
published-head required CI. Its host results do not change the target gate.

### Native evidence and reproduction

The native validation clone starts at `3324e7c5acdfef0a7046e5a1520f8a981c4900ef`.
Main integration does not change its native renderer or standalone SVG graph.
For the baseline negative controls, only `RnsvgItems.cpp` and `scene/Bounds.cpp`
were replaced with their contents at `d38097f38d5a1ebb560627649e95c84d04d32f02`;
the new tests remained in place. Both assertions failed for their intended
reasons: the visible root did not repaint after removal, and the nested group
bounds were empty instead of `QRectF(20, 20, 60, 60)`. The two source files were
then restored from `3324e7c`, and a clean tracked-source diff was verified before
the candidate rebuild. Both regressions passed in 15 ms.

The initial full candidate run passed seven of eight CTests. The filter test
failed the unchanged first convolution pixel assertion; all filter golden
comparisons passed. That run is retained alongside the subsequent result so
the fixture fix has explicit before/after evidence. After capturing the size
before moving the kernel, all eight CTests passed in 34.86 seconds and the
focused convolution test passed in 5 ms. One direct invocation omitted the
offscreen platform and failed to initialize XCB before tests started; it was
rerun with the same offscreen/software settings that CTest already specifies.

| Evidence                                                                                                                                                            | Observation                                                                                                 |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| [Native build configuration](../evidence/pr161-nested-roots-2026-11-14/native-config.txt), [configure log](../evidence/pr161-nested-roots-2026-11-14/configure.log) | Actual compiler, Qt version, sanitizer flags, test environment and tested source hashes.                    |
| [Baseline build](../evidence/pr161-nested-roots-2026-11-14/baseline-build.log), [candidate rebuild](../evidence/pr161-nested-roots-2026-11-14/candidate-build.log)  | Standalone renderer and all eight test executables build; original sources then restored candidate sources. |
| [Original native source with new tests](../evidence/pr161-nested-roots-2026-11-14/baseline-negative.log)                                                            | Both intended assertions fail; program exit 2.                                                              |
| [Restored candidate source with new tests](../evidence/pr161-nested-roots-2026-11-14/candidate-focused.log)                                                         | Both nested SVG regressions pass.                                                                           |
| [Initial full native run](../evidence/pr161-nested-roots-2026-11-14/candidate-ctest.log)                                                                            | Seven of eight pass; convolution fixture fails its original pixel assertion.                                |
| [Fixture rebuild](../evidence/pr161-nested-roots-2026-11-14/filter-test-fix-build.log)                                                                              | Filter test compiles and links after the setup fix.                                                         |
| [Focused convolution test](../evidence/pr161-nested-roots-2026-11-14/filter-test-fix-focused.log)                                                                   | Original assertions pass with the valid kernel.                                                             |
| [Full native run after fixture fix](../evidence/pr161-nested-roots-2026-11-14/fixed-candidate-ctest.log)                                                            | Eight of eight pass; no ASan/UBSan report.                                                                  |
| [Missing-offscreen diagnostic](../evidence/pr161-nested-roots-2026-11-14/filter-missing-offscreen.log)                                                              | Configuration failure before test execution; corrected for the focused rerun.                               |

Configure and build the standalone graph with the exact Qt prefix available:

```sh
cmake -S tests/native/qt-rnsvg -B build/rnsvg-sanitized -G Ninja \
  -DRNQNX_ENABLE_QT_RNSVG=ON \
  -DRNQNX_QT_RNSVG_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++
cmake --build build/rnsvg-sanitized --parallel 2
ctest --test-dir build/rnsvg-sanitized --output-on-failure
```

The renderer library and test executables use ASan/UBSan, with
`-fno-sanitize-recover=undefined`, frame pointers and warnings as errors.
The Qt shared libraries are the uninstrumented Qt 6.11.1 release build.
CTest retains the repository's software/offscreen backend, 120-second test
timeout, `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, and
`UBSAN_OPTIONS=halt_on_error=1`. This run does not test leaks, the full
Hermes/Fabric renderer graph, a QNX cross-build or a device.

The [staging fixture log](../evidence/pr161-nested-roots-2026-11-14/main-integration-staging.log)
records the 17 passing host cases; the independent staging reviewer also ran
all 17 successfully against the same script and test blobs.
The configure and missing-offscreen diagnostic logs have trailing whitespace
normalized; their original files remain in the native validation workspace.

At the original head, [renderer run 43542289743](https://github.com/mikevocalz/react-native-qnx/actions/runs/43542289743)
passed its OFF configuration but timed out on corpus shards 1 and 2 in the ON
configuration, each at the existing 300-second limit while mounting batches
13 and 14. The other eight workflows passed. The production native source
was unchanged from the earlier passing `a52ffff` head. A targeted hosted
rerun is separate evidence; this fix does not claim to explain or repair the
corpus timing. No timeout, fixture, pixel tolerance or required gate changes.
The original failure's [log excerpt](../evidence/pr161-nested-roots-2026-11-14/original-ci-excerpt.txt)
retains its timestamps and job identity.

At `13a0e49ae7ea6b16faa23840926d3976866f6646`, all nine hosted workflows
completed successfully, observed at 17:36:28 UTC on 2026-11-14. The
[full renderer run 43616638798](https://github.com/mikevocalz/react-native-qnx/actions/runs/43616638798)
passes both OFF and ON configurations. The ON job passes 49/49 integrated
CTests; its four corpus shards take 4.50, 197.26, 197.85 and 5.32 seconds,
within the unchanged limits. Its separate SVG ASan/UBSan step passes 8/8,
and the HTTP sanitizer step passes 1/1. The
[workflow snapshot](../evidence/pr161-nested-roots-2026-11-14/published-head-ci.json)
and [timestamped ON job excerpt](../evidence/pr161-nested-roots-2026-11-14/published-renderer-on-excerpt.txt)
identify the exact head and results. All three existing review threads remain
resolved. This successful run does not establish the cause of the earlier
corpus timeouts, and it predates the packaging-only follow-up described above.

## Skills and review

The SVG integration agent owns the two native paths and scene regressions in
the Qt Quick / Fabric and QNX regression roles. The coordinating Codex agent
independently reviewed the two native changes and regressions with no source
blocker. The separate hardening reviewer compared the staging merge with
current main and PR 163, found no blocker, and independently reran all 17
fixtures. The reviewed staging script blob is
`4639514fc507a1c33ed56e13edef987daaa922d4`; its test blob is
`a740f8deada3f39ae6e378d5975301b167b1f737`. These are agent source reviews and
host tests, not external service or target acceptance.
External CodeRabbit CLI authentication in this Cloud environment returned
`environment_unsupported`; no new service review is claimed.

| Source read                                               | Revision                                   | Applied decision                                                                                                     |
| --------------------------------------------------------- | ------------------------------------------ | -------------------------------------------------------------------------------------------------------------------- |
| Repository `qnx-native-api` and `qnx-platform-validation` | Base above                                 | Preserve the existing owner thread and renderer; label host and target evidence separately.                          |
| Margelo `api-design` → `build-nitro-modules` → `cpp`      | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Read in order; preserve the public API and generated bindings, use the existing item lifetime and bounded traversal. |
| Callstack `react-native-best-practices`                   | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Require measured evidence before changing the corpus or optimizing the mounting path.                                |

Upstream skills were read through the connected source provider; no local
skill installation or upstream author participation is claimed. The locked
React Native, SVG and Nitro interfaces were inspected locally after install.

## Security, limitations and rollback

The native fixes add no I/O or permissions; nested bounds retain the existing
depth cap. The staging merge preserves guarded local filesystem operations
and rollback while explicitly bounding the optional SVG source. No premium
payload, SDK files or redistributed Qt artifacts are added to this commit.
No production performance budget or release gate is promoted. Existing
device receipts stay attached to their original revisions. The fresh combined
QNX build and target acceptance recorded in the PR remain outstanding.

Reverting this continuation restores the preceding invalidation and nested
bounds behavior. Rebuild the native app if deploying either revision; no
JavaScript API or bundle-format migration is involved. Reverting the staging
merge must preserve PR 163's transactional guarantees. Exact-head required
hosted CI and the existing combined QNX target qualification remain acceptance
requirements. The changed native code has no fresh QNX/device run; this receipt
does not promote the PR to ready or satisfy the repository's target gate.
