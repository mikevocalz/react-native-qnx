# Optional Qt software-capture recipe, 2026-11-22

Base: PR #218 at `2e89cf3afb139ab74e1f433f1913d934641ed89c`, on release
baseline `ae0040d34343e5efc95017e655efd594fda6cc4a`. The owner selected option C
and confirmed the release board and licensed self-hosted runners are unavailable.

## Result

`QNX_SOFTWARE_GRAB=1` selects the existing software-grab patch in addition to
the two default QNX patches. Unset or `0` preserves the default patch set.
Other values fail before patching. The source manifest records the selected
variant and refuses to contradict an existing applied-patch stamp. Runtime
package manifests must separately record the actual plugin hash and patch set.
The native patch itself, renderer, native APIs, dependency graph, feature
defaults, window dimensions and boot configuration are unchanged.

The PR #218 driver did not enable Bash errexit and continued after a failed
clean-tree preflight. It now explicitly propagates that failure. The regression
checks that no stage starts and no timing log is created after rejection.
The README's extra blank line, which failed hosted formatting, is also removed.

## Validation

All results here are **host fixtures**, macOS, Node 26.13.0. Installed dependencies
use the frozen lockfile and pnpm 12.21.0 in an isolated temporary checkout.

| Command                                                                                               | Result                                                                                                                                                                                                                          |
| ----------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `bash tests/scripts/qt-public-clean-tree.test.sh`                                                     | Pass, including caller failure propagation                                                                                                                                                                                      |
| `bash tests/scripts/qt-public-patch-selection.test.sh`                                                | Pass, real git apply against synthetic old-side source hunks; default/0/1, exact stamps, repeated calls, existing regular/symlink stamps, nine invalid values, failed preflight before mutation and source-offer stamp mismatch |
| `python3 -I tests/python/test_qt_public_source_bundle.py`                                             | Pass, two tests with five variant cases; actual bundle script in a synthetic repository verifies variant metadata, recipe/archive/CVE hashes and mismatch refusal. Only the fixture copy uses synthetic archive pins            |
| `bash -n` on changed shell scripts/tests                                                              | Pass                                                                                                                                                                                                                            |
| `shellcheck -x --source-path=SCRIPTDIR` on selection, patching, source-bundle scripts and shell tests | Pass                                                                                                                                                                                                                            |
| `node tests/js/release-units.test.mjs`                                                                | Pass                                                                                                                                                                                                                            |
| `node scripts/validate-docs.mjs`                                                                      | Pass, 24 components and 11 gates                                                                                                                                                                                                |
| `node scripts/validate-dependencies.mjs`                                                              | Pass                                                                                                                                                                                                                            |
| `prettier --check .` using pinned 3.10.1                                                              | Pass                                                                                                                                                                                                                            |
| `node scripts/validate-docs.mjs --release`                                                            | Expected failure: ten gates pending                                                                                                                                                                                             |

The patch-selection fixtures are not a fresh upstream Qt source audit or a
compiled plugin. No actual Qt runtime was built, staged or run, and no physical
device or self-hosted lane was exercised. The readiness record remains 1/11
passed. A final clean build, source-offer review, package publication,
independent reproduction and all other gate acceptance remain required.

## Review and skills

DRI: primary agent, release/dependency role. Independent reviewer: QA/release
audit subagent. Review found an existing-stamp guard whose combined Bash AND
list could bypass errexit; separate guards and stamped-pristine controls fix it.
The reviewer found no further functional blocker in the implementation diff.

| Source                                                                 | Read and decision                                                                                                                                  |
| ---------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| Repository `qnx-platform-validation` at base above                     | Applied: host fixtures do not promote cross-build, VM or physical gates                                                                            |
| Repository `qnx-native-api` at base above                              | Scope inspected; no native API or patch-body change                                                                                                |
| Callstack `github-actions`, `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Source inspected; its simulator artifact recipes do not apply to QNX. Existing pinned read-only shell workflow is retained and runs the regression |

No skill installation or vendor certification is claimed. Native API design,
Nitrogen generation, runtime ABI changes and native performance tuning are
outside this shell recipe change. QNX regression evidence is limited to default
patch selection; Marathon is unaffected. Existing Qt licensing boundaries and
the source offer's draft status remain unchanged.

## Rollback and next qualification

Revert this continuation to remove the opt-in and source metadata. Do not reuse
an extracted tree when changing variants. Build each variant from empty roots,
then audit, stage, record hashes and run the resulting plugin on its named
target. No publication, deployment, merge, owner source-offer approval or
hardware operation is performed by these tests.
