# SVG layout subscription evidence

Read the [validation receipt](../../validation/pr161-svg-layout-interest-2026-11-17.md)
for the implementation, measured results, limits and rollback. This directory
preserves Linux host evidence from the exact published PR 161 parent and its
reviewed continuation. It does not contain current QNX/device qualification.

## Final candidate

| Record                                                                                                                                                                     | Purpose                                                                                                          |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| [Full CTest output](final-candidate-full.log), [native output](final-candidate-full-last-test.log), [JUnit](final-candidate-full.xml), [result](final-candidate-full.json) | All 49 tests and 149 integrated pixel comparisons pass; the four corpus limits remain 300 seconds.               |
| [Source before execution](final-candidate-source-before.json), [final source patch](final-reviewed-source.patch), [patch identity](final-reviewed-source-patch.json)       | The 23 reviewed source/generator/test files match the primary clone and remain unchanged during execution.       |
| [Artifact identities](final-artifact-identities.json)                                                                                                                      | Restored reporter/test objects and public Metro bundle match the earlier passing candidate.                      |
| [CTest inventory](final-candidate-ctest-inventory.json), [configuration](final-candidate-configuration.json)                                                               | Actual test membership, timeouts, flags and Hermes slow debug configuration.                                     |
| [Final native build](final-candidate-build.log), [build result](final-candidate-build.json)                                                                                | Ordinary restored build completed successfully.                                                                  |
| [Final local checks](final-light-checks.txt)                                                                                                                               | Commands and terminal statuses for dependency, architecture, type, codegen, documentation and formatting checks. |
| [File hashes](artifact-sha256.txt)                                                                                                                                         | SHA256 inventory of the committed evidence files; the inventory excludes itself.                                 |

## Separate controls and discoveries

| Record                                                                                                                                                                                   | Scope                                                                                                                                      |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| [Original public regression](baseline-public-regression.json), [CTest output](baseline-public-regression.log), [native output](baseline-public-regression-last-test.log)                 | Exact original production with only the public fixture/test change; five intended native-dispatch failures.                                |
| [Saved public test](public-fabric-test.cpp.txt), [saved JS fixture](public-fixture.js.txt)                                                                                               | Byte-preserved source copies for the original-compatible control; text suffixes avoid incidental formatting.                               |
| [Original shard timing](baseline-heavy-shard.json), [output](baseline-heavy-shard-last-test.log)                                                                                         | One local original-source heavy-shard comparison, 199.36 seconds.                                                                          |
| [Native marker without transport](native-marker-without-transport.json), [native output](native-marker-without-transport-last-test.log)                                                  | Failed intermediate candidate; the real public callback times out.                                                                         |
| [Original JS transport](transport-original-negative.json), [output](transport-original-negative.log), [corrected transport](transport-candidate.json), [output](transport-candidate.log) | Identical real pinned-pipeline test against original/corrected QNX BaseViewConfig.                                                         |
| [Corrected public native result](candidate-public-regression.json), [output](candidate-public-regression-last-test.log)                                                                  | Same public test source and fixture as the original control; real Metro/Hermes callback and all 14 marker classes pass.                    |
| [Complete native-interest candidate](candidate-native-interest.json), [output](candidate-native-interest-last-test.log)                                                                  | Public listener plus late native registration, dynamic interest, self-clear, weak ownership and reporter lifetime.                         |
| [Explicit lifetime mutation](lifetime-mutation.json), [source identity](lifetime-mutation-source.json), [delta](lifetime-mutation.patch), [output](lifetime-mutation-last-test.log)      | Separate candidate mutation, not original-source evidence; removing the final lifetime check produces the intended extra-dispatch failure. |

Candidate transfer initially preserved older file timestamps. The refreshed
build logs retain that correction and the compiler's four filter-table sentinel
diagnostics. The final reviewed source uses `nullptr` for those absent
callbacks. The older frozen candidate patches and their manifests remain as
intermediate provenance; `final-reviewed-source.patch` is the complete final
source delta.

## Build and bundle provenance

- [Fresh baseline setup](baseline-setup-setup-result.json),
  [configure command](baseline-setup-configure-command.json),
  [configure output](baseline-setup-configure.log),
  [cold build](baseline-cold-build.log), and
  [verified dependency sources](baseline-setup-dependency-source-proof.json).
  Dependency source reuse does not imply compiled-object reuse.
- [Nine application bundle inventory](transitive-bundle-refresh-plan.json),
  [original bytes/timestamps](transitive-bundle-refresh-before.json),
  [initial freshness failure](initial-freshness-check-result.json),
  [five stale outputs](transitive-bundle-refresh-initial-stale.json),
  [exact Metro refresh commands/results](transitive-bundle-refresh-direct.json),
  and [final bundle identities](transitive-bundle-refresh-after.json).
- [Initial build/freshness wrapper](final-build-and-freshness-check.py) and
  [final Metro/CTest wrapper](final-metro-refresh-and-ctest.py) retain the actual
  working paths used for this run. They are evidence helpers, not production
  build or test targets. [Focused CTest capture](run-focused-ctest-receipt.py)
  records the earlier separate controls.
- [Published parent CI summary](published-d3-ci-summary.json) and
  [failed renderer-ON job](published-d3-renderer-on.log) preserve the earlier
  47/49 result. New local results do not change that hosted outcome.

The raw logs retain nonfatal upstream filter conversion and shutdown messages.
The full local [JavaScript runner](js-tests.log) stops at the existing Marathon
Unix-socket `EPERM`; it is not a passing full-suite result. Existing
[transport](transport-candidate.log), [overlay](svg-js-render.log),
[resolver](svg-js-resolver.log), [packing](svg-js-packing.log) and
[codegen](codegen-check.log) records keep their separate scopes.

JSON receipts may be formatted for repository style without changing their
values. Raw logs, patches, saved fixture text and compiled-artifact hashes are
preserved. The final inventory hashes the committed evidence representations.
