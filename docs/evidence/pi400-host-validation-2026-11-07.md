# Pi 400 follow-up host validation, 2026-11-07

The full macOS host graph now builds and passes **11/11 CTests**, including
Hermes, real React/Fabric rendering and bad-bundle rejection, the viewport plus
fixed-child regression, the original JavaScript error-forwarding assertions,
and the decorated-View lifecycle regression. This is host evidence; the QNX
candidate separately passed its Nitro v1 marker, actual rendered screenshot
and exit zero before promotion to the normal device launch path.

The first full run found two inner failures: the new viewport script reused a
binding from another Hermes evaluation, and static JSI was linked into both
Hermes and its consumer, duplicating `JSError` RTTI on Darwin. The viewport
script now resolves its own block-local bindings. Host Hermes now uses shared
JSI; `otool -L` confirms both Hermes and its consumer load `libjsi.dylib`.
The original error test passes without weakening the expected message. The
QNX Hermes library and its JSI linking were not changed by this host-only fix.

Configuration used a fresh shallow checkout of Hermes commit
`9a115f15785851f4ff2747ebb090ddd066fc8ad2`, canonical unpatched host RN sources,
Qt 6.12.0, MinSizeRel and enabled host sanitizers. The full renderer gate was
required, preventing an accidental smaller test graph:

```sh
cmake -S . -B build/macos-full-host-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_TESTING=ON \
  -DRNQNX_ENABLE_HOST_SANITIZERS=ON -DRNQNX_REQUIRE_FULL_HOST_RENDERER=ON \
  -DRNQNX_BUILD_HERMES=ON -DRNQNX_FETCH_REACT_NATIVE_DEPS=ON \
  -DFETCHCONTENT_SOURCE_DIR_RNQNX_HERMES=/path/to/pinned-hermes-checkout
cmake --build build/macos-full-host-tests --parallel 4
ctest --test-dir build/macos-full-host-tests --output-on-failure --timeout 120
```

## Mutation benchmark

The optional `tests/benchmarks` harness times mutations only, after creating
items. Each run validates sibling order, removal completion and preservation
of the unrelated subtree child count. Creation, rendering, GPU and JavaScript work are
excluded. Decorated ordering is checked by the separate correctness regression;
the old baseline is incorrect for decorated parents and is not a valid visual
comparison. All three implementations were compiled against identical headers,
Qt, compiler and optimization flags. Three interleaved processes per variant,
each discarding run zero and measuring five runs, produced 15 warm samples.

Median milliseconds for 1,000 nodes/operations:

| Workload                                             | Before decoration fix (`f67214c`) | Registry scan (`6a6a127`) | Cached pointer membership |
| ---------------------------------------------------- | --------------------------------: | ------------------------: | ------------------------: |
| Insert 1,000 siblings                                |                             0.214 |                    18.708 |                     5.417 |
| Remove 1,000 siblings                                |                             0.203 |                    18.915 |                     6.050 |
| 1,000 insert/remove pairs with 1,000 unrelated nodes |                             0.418 |                    25.601 |                     0.604 |

The cached index removes the per-mutation scan of every registered item.
Flat siblings still require filtering their actual Qt order: insertion/removal
remain roughly 25/30 times the old undecorated baseline. This is a measured
remaining cost, not baseline parity. No QNX FPS, latency, startup or RSS claim
follows from these host numbers. Sample minima, medians, maxima, platform and
source hash are in [the measurement record](pi400-host-mounting-2026-11-07.json).

Reproduce a single current-profile process (run zero is warm-up):

```sh
cmake -S tests/benchmarks -B build/mounting-benchmark -G Ninja \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build/mounting-benchmark --parallel 2
QT_QPA_PLATFORM=offscreen build/mounting-benchmark/rnqnx_mounting_benchmark
```

For comparisons, save each revision's mounting implementation with `git show`
and configure a separate build directory with
`-DRNQNX_MOUNTING_SOURCE=/absolute/path/to/saved.cpp`. Use the same current
headers and toolchain for all variants, interleave process order and exclude
creation consistently. The fixture checks remain active in release builds.
