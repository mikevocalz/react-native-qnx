# Public Qt staging: preserve the previous runtime

Gate: `distribution`, still pending. Base:
`f87c742a423e5c9fda61dfa837b629a7a2dd5307`. Lockfile SHA-256:
`5d8a369d8bc0102bbb6984567646041596994ab5f816749732244178b8d9a6ff`.

## Change and boundary

The previous staging script deleted its output before checking the first input.
The missing-library negative control loses the known-good file and symlink;
see [the baseline failure](staging-baseline-negative.log). Staging now copies
into a sibling candidate, validates the existing dependency closure, writes
manifests, and promotes only after those steps succeed. A failed promotion
restores the previous directory. Copies reject paths escaping the selected
public source/build trees or candidate. Output paths cannot replace the
repository, Qt source/build trees, their ancestors, or symlink targets.

The per-output lock prevents concurrent staging. Its journal supports manual
recovery after termination. Publication uses two renames and can leave the
output temporarily absent on interruption. The
[recovery procedure](../../../scripts/qt-public/README.md#staging-failures-and-recovery)
states this limitation and how to restore the exact recorded previous runtime.
No automatic stale-lock takeover or crash-atomic publication is claimed.

Changed implementation: `scripts/qt-public/stage-runtime.py`. Regression
fixtures: `tests/python/test_qt_public_staging.py`, run automatically through
`tests/js/qnx-public-runtime-staging.test.mjs`. The runtime library/plugin set,
optional audit interface, dependency pins and Linux/Wayland code are unchanged.
This does not resolve audit completeness, add-on staging, corresponding-source
distribution, binary RPATH, target-library identity or release attestation.

## Source and skills

Read the staging/audit scripts, public Qt build README, ADRs 0013/0017/0018,
packaging policy and public Qt receipts. The Qt reference exploration read the
section 9.8 manuals; selected API/source bodies are distinguished from complete
manual audits. [Qt's LGPL text](https://doc.qt.io/qt-6/lgpl.html) informs the
existing notice/source/replacement boundary. This change makes no new legal
compliance determination and copies no SDK or Qt binary into the repository.

| Skill                     | Version/source read                           | Decision applied                                                                                                                     |
| ------------------------- | --------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| `qnx-platform-validation` | Repository at base above                      | Keep host fixtures separate from cross-build/device evidence; retain failed control.                                                 |
| `no-ai-slop`              | Installed local SKILL.md, no declared version | State actual outcomes and remaining gates.                                                                                           |
| `testing-strategy`        | Local engineering plugin 1.2.0                | Test data loss, isolation, interruption and repeatability through the real script.                                                   |
| `documentation`           | Local engineering plugin 1.2.0                | Document exact recovery and escalation conditions.                                                                                   |
| `deploy-checklist`        | Local engineering plugin 1.2.0                | Keep hashes, attestation, device qualification and rollback separately unchecked.                                                    |
| `code-review`             | Installed skill 0.1.0; CLI 0.7.6              | Independent review and correction of the interruption finding. External CodeRabbit review unavailable because the CLI is signed out. |

The three engineering skill files were read from
`~/.claude/plugins/cache/knowledge-work-plugins/engineering/1.2.0/skills/`.
No C++ or public native API changed; the Margelo native implementation sequence
does not apply to this packaging-only slice.

## Validation and independent review

Command: `node tests/js/qnx-public-runtime-staging.test.mjs`.
[Raw host output](staging-host.log): 13 fixtures pass. These include missing
library/plugin, malformed audit, failed readelf, missing Qt dependency,
source/output path escape, protected destinations, lock contention, failed
promotion, manifest hashes, byte-identical repeat staging, and SIGTERM between
renames followed by manual recovery. Files and ELF responses are synthetic;
these numbers are fixture counts, not Qt build or device results.

DRI: primary agent acting as Release / dependency engineer. Independent
reviewer: `review_distribution`, acting as Security / QA and release reviewer.
Its P2 interruption finding was addressed with the journal, runbook and
subprocess test; re-review reports no further blocking finding. The reviewer
ran the host fixtures independently. No vendor participation is implied.

Host native builds, ASan, QEMU, QNX cross-build, Pi 400, stock Pi 4 QSTI,
performance and power-loss tests were not run for this tooling slice. Colima
was stopped when inspected. No target was launched, stopped or reconfigured.
No scheduling, window dimensions, input, boot, renderer or app state changed.
Frozen installation with pnpm 12.18.0, dependency validation, typechecking,
documentation validation, the existing runner-manifest fixtures and the
Marathon Metro platform regression passed locally. Command exit statuses and
raw outputs are in [command-results.json](command-results.json). The runner
fixtures use a local loopback server and synthetic archives; they do not
qualify a real runner. Hosted CI belongs to the published head and must be
checked separately.

## Release checklist and rollback

- [ ] Re-run the public Qt build/audit/staging against the exact selected source and flags.
- [ ] Inspect all selected module/plugin notices and corresponding-source materials.
- [ ] Record actual runtime/archive hashes and complete library/plugin closure.
- [ ] Verify SDK, target-image and evaluation-file exclusion from the runner archive.
- [ ] Verify replacement/relinking instructions and binary RPATH.
- [ ] Pass exact-head required CI and attach the resulting raw logs.
- [ ] Verify release attestations separately from local hashes.
- [ ] Qualify the exact target image and libraries on a clean device.
- [ ] Preserve the previous runtime and test the package rollback procedure.

Revert this PR to return the previous staging script. Before any staging
rollback, restore an interrupted output using its own journal; do not remove a
lock while its process is running. A reverted script again has the data-loss
behavior reproduced by the negative control, so preserve a separate verified
runtime before using it.

Next distribution work: audit the newly indexed
`CVE-2026-105943-qtbase-6.12.diff` and its applicability, complete selected
add-on notices/staging and source/replacement materials, then create an archive
for validation. Its presence in the
[public Qt index](https://download.qt.io/official_releases/qt/6.12/)
does not establish that the current binary is affected or that a patch has been
applied. No v1 gate is promoted by this receipt.
