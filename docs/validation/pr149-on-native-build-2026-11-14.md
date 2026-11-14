# PR 149 native integrations ON — 2026-11-14

## Result and tested source

Local source commit `4d13330ccc18438d4e7a3c38128886feaa6525ca`, tree
`2f9479e2b37c8d6fec673d2d6316c641f2044e6b`, builds the Worklets, Reanimated
and Gesture Handler static libraries and the integrated QtReactHost executable.
All **60 enabled QtReactHost cases pass**, with one CTest target, no reported
skips and no AddressSanitizer diagnostic, in 68.00 seconds. The source starts
at the combined PR 149 candidate `2f3f208e2d925b42ec8b3ce2e60f7db725bd67c0`;
the only additional production changes are 26 lines in the two CMake adapters.
The regression test source is unchanged. These are local validation identities;
publication may assign a different commit to the same source blobs.

The [phase results](../evidence/pr149-on-native-build-2026-11-14/phase-results.json),
[host log](../evidence/pr149-on-native-build-2026-11-14/host-test.log) and
[tested source/artifact inventory](../evidence/pr149-on-native-build-2026-11-14/tested-source-and-artifacts.json)
record the actual commands, outcomes, 502 source-file hashes and final artifacts.
This continuation adds integrated host and static-library compile evidence to
the separate [shutdown sanitizer receipt](pr149-shutdown-2026-11-14.md).
All three feature switches still default OFF in source.

## Compiler findings and bounded adapters

Enabling the three existing native targets exposed missing standard headers
in the exact installed packages. Each failure is retained in a separate raw
log; later successful stages do not replace those observations.

| Installed source                                      | Observed omission                                                                           | Adapter                                                                                   |
| ----------------------------------------------------- | ------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| Worklets 0.13.1 `RunLoop/EventLoop.cpp`               | `remove_if` and `upper_bound` without `<algorithm>`                                         | Include the header for this translation unit only                                         |
| Worklets 0.13.1 `Tools/JSISerializer.cpp`             | `find` without `<algorithm>`                                                                | Include the header for this translation unit only                                         |
| Worklets 0.13.1 `Tools/WorkletsJSIUtils.cpp`          | `istream_iterator` without `<iterator>`; inspection also found `copy` without `<algorithm>` | Include those two headers for this translation unit only                                  |
| Reanimated 4.7.1 `FilterOp.h` and `TransformOp.h`     | `uint8_t` without `<cstdint>`, affecting several consumers                                  | Generate copies containing the standard include followed by the unchanged original header |
| Reanimated 4.7.1 `FilterOp.cpp` and `TransformOp.cpp` | `invalid_argument` without `<stdexcept>`                                                    | Include the header for those two translation units only                                   |
| Reanimated 4.7.1 `vectors.cpp`                        | `hypot` without `<cmath>`                                                                   | Include the header for this translation unit only                                         |

The Worklets algorithm scan found five affected users: `TextDecoding.cpp` and
`SerializableWorklet.cpp` already include `<algorithm>`; the three files above
do not. Reanimated's generated enum copies use the existing override include
directory, which precedes the package directory. All inspected enum-header
consumers use the full `reanimated/...` path, so they resolve the generated
copies. The original header text is preserved byte for byte after the include
prefix. Existing generated platform adaptations retain their order and behavior.

No installed package file, dependency version, lockfile, target-wide include
or warning policy changed. Existing upstream warning settings remain; project
translation units retain strict warnings. The
[Worklets source/hash record](../evidence/pr149-on-native-build-2026-11-14/worklets-algorithm-fix.json),
[Reanimated source/hash record](../evidence/pr149-on-native-build-2026-11-14/reanimated-filter-headers-fix.json)
and [adapter diff](../evidence/pr149-on-native-build-2026-11-14/native-header-adapters.patch)
identify the inspected bytes and changes. Final
[Worklets](../evidence/pr149-on-native-build-2026-11-14/worklets-standard-header-compile-flags.txt)
and [Reanimated](../evidence/pr149-on-native-build-2026-11-14/reanimated-final-header-compile-flags.txt)
Ninja command records confirm the intended per-source flags. Package hashes
were checked again after compilation and remained unchanged.

## Validation environment and execution

The Linux x86-64 Debug build uses Clang 19.1.7 with libstdc++ 14, Qt 6.11.1,
CMake 3.31.10, Ninja `1.13.2.git.kitware.jobserver-pipe-1`, Node 26.13.1 and
pnpm 12.18.0. Exact toolchain/package records are in
[toolchain.json](../evidence/pr149-on-native-build-2026-11-14/toolchain.json).
React Native 0.88.1, Hermes commit
`9a115f15785851f4ff2747ebb090ddd066fc8ad2`, Worklets 0.13.1, Reanimated 4.7.1,
Gesture Handler 3.4.0 and SVG 15.16.0 retain their locked identities.

The existing Clang/Hermes renderer cache was reused at the same source path
after the PR 159 owner released the checkout. Its tested executable, bundle
and evidence were preserved separately before switching branches. Frozen pnpm
installation selected the PR 149 dependency graph without changing the lock
or downloading packages. The playground bundle was generated again from this
checkout with two Metro workers; its resulting bytes match the earlier bundle.
Native parallelism is four and Qt autogen parallelism is two.

The effective ON configuration and exact dependency paths are preserved in
[CMakeCache.final.txt](../evidence/pr149-on-native-build-2026-11-14/CMakeCache.final.txt).
The core commands were:

```sh
pnpm install --frozen-lockfile
cmake -S . -B build/host-review159-clang \
  -DCMAKE_AUTOGEN_PARALLEL=2 \
  -DRNQNX_ENABLE_WORKLETS=ON \
  -DRNQNX_ENABLE_REANIMATED=ON \
  -DRNQNX_ENABLE_GESTURE_HANDLER=ON
cmake --build build/host-review159-clang \
  --target rnqnx_worklets rnqnx_reanimated rnqnx_gesture_handler qt_react_host_host_test \
  --parallel 4 -- -k 0
ctest --test-dir build/host-review159-clang \
  -R '^qt_react_host_host_test$' --no-tests=error --timeout 180 \
  --output-on-failure -V
```

The build directory name reflects cache reuse; its final source is the PR 149
candidate identified above. Ninja's keep-going option allowed independent
objects to finish while collecting actual errors and still returned failure
when any requested target failed. The final build returned zero. The host test
uses Qt's offscreen/software backend and the existing executable's address
sanitizer instrumentation, with leak detection disabled. This target does not
enable UBSan, and not every linked library object is sanitizer-instrumented.

The 60 cases include the integrated mount-hook ordering regression,
`turboModuleInstancesSurviveTemporaryLookupsAndGarbageCollection`,
`retainedSourceCodeTracksSubsequentEvaluations` and
`imageLoadedForwardingDoesNotRequireLoaderLookup`, plus the existing real
Metro rendering, input, image and runtime cases. The exact names appear in
[host-test-result.json](../evidence/pr149-on-native-build-2026-11-14/host-test-result.json).
This continuation ran one CTest executable, not the entire CTest project.

Adding a higher-priority generated header does not automatically invalidate
every existing object that previously resolved the original header. A full
dependency audit found 15 such Reanimated objects in the reused cache. They
were explicitly recompiled, and the requested targets passed again. The final
[dependency audit](../evidence/pr149-on-native-build-2026-11-14/enum-header-dependency-audit-final.json)
examined all 113 Reanimated objects: 20 record the generated enum headers and
none still record the original enum headers. The tested host executable's hash
remained unchanged through this archive refresh, so the completed execution
still identifies the final host artifact.

| Artifact                           |     Bytes | SHA-256                                                            |
| ---------------------------------- | --------: | ------------------------------------------------------------------ |
| Executed `qt_react_host_host_test` | 146622328 | `f22929640319f7678fe7e6ab03ee7cd04cbe32dd5d0ce22799be10c39cea3ca8` |
| Real playground Metro bundle       |    784360 | `761ef40b9100ae5eec2a9ee830e4a24a33d45554bf7a9784673c27cc5b6ea817` |
| `librnqnx_worklets.a`              | 261585810 | `6fa2ba8645196f6cdbe14795c4d844d23b8829e79c5df0fdc6bd3f0ae7073ee2` |
| `librnqnx_reanimated.a`            | 345019438 | `5083dab889519f9b7075b4306166b8759e0f1a03dd94b3190e05c19618af253f` |
| `librnqnx_gesture_handler.a`       | 240635752 | `8cd723cf43997b4595ddb74d49465c5fa181a8a997c00e6bfceaeb15f0c2f875` |

## Review, boundaries and next acceptance

The native-validation agent owns the two CMake changes and this evidence.
A separate SVG/native review agent inspected the complete adapter, original
headers, generated prefix preservation, include precedence and final Ninja
flags, with no blocking finding. The Expo review agent independently checked
the Worklets omission and identified the matching TransformOp omission before
the keep-going compile confirmed it. No CodeRabbit service review is claimed.

The repository `qnx-native-api` and `qnx-platform-validation` instructions were
applied. The ordered Margelo `api-design` → `build-nitro-modules` → `cpp` source
at `1e9e17be6f41c838db472d6a4b943fc26355a3be` and Callstack React Native guidance
at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` informed the existing API and
evidence boundary: repair the pinned integration adapters, retain Fabric,
Hermes and Qt Quick, and distinguish host compilation from target behavior.

The three optional integrations are static archives. This host executable
links `rnqnx_react_host`; it does not install or execute an integrated Worklets
application. The result does not establish a current QNX SDP build, QNX VM,
Marathon compositor run, device input, busy-JS responsiveness, frame latency
or release-artifact acceptance. Earlier device observations remain scoped to
their historical receipts. Required hosted checks must still pass on the
final published head. No pending v1 or device gate is promoted by this result.

The change introduces no public API, runtime permission, premium payload or
dependency/license change. No performance budget is claimed. Rollback is the
two CMake adapter changes followed by regeneration of the build; feature
defaults remain OFF. Keep the separate shutdown fixes and their evidence.
The next acceptance step is final-head hosted validation and the outstanding
target/application trials already tracked in the shutdown and frame receipts.
