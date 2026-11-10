# Check which candidate an evidence receipt belongs to

This source-checkout tool freezes the committed candidate and checks local receipt
files against it. You need Git, the repository's supported Node version and clean
tracked files. It does not run tests, contact devices or approve a release.

From the repository root:

```bash
node scripts/qnx-release-dossier.mjs create --file build/release-candidate/dossier.json
node scripts/qnx-release-dossier.mjs verify --file build/release-candidate/dossier.json
```

Creation refuses an existing destination. Verification does not modify source,
receipts or artifacts. Both commands return nonzero for invalid inputs; successful
verification means **file integrity**, not completed release qualification.
Typed boot/performance receipts also report capture completeness. Incomplete
captures still exit 0 when integrity checks succeed; inspect the JSON report.

## Add an observed receipt

The generated JSON records the exact HEAD commit, tree, lockfile, package manifest,
component matrix and existing seven release gates. Its `receipts` array starts empty.
Do not fill it with invented results. Place authorized logs or capture files under
`build/release-candidate/artifacts/`, then add a receipt using this shape:

```json
{
  "id": "your-unique-receipt-id",
  "gate": "ci",
  "lane": "hosted-ci",
  "outcome": "passed",
  "commit": "COPY_THE_ACTUAL_TESTED_FULL_COMMIT",
  "lockSha256": "COPY_THE_TESTED_LOCKFILE_SHA256",
  "command": "The exact command that was executed",
  "environment": "The actual host, toolchain and feature flags",
  "limits": "What this observation does not prove",
  "author": "The receipt author's identity",
  "reviewer": null,
  "artifacts": [
    {
      "path": "artifacts/actual-test.log",
      "bytes": 123,
      "sha256": "REPLACE_WITH_THE_ACTUAL_FILE_SHA256"
    }
  ]
}
```

This is a schema illustration, **not valid evidence**. Replace every placeholder
and the example byte count with observed values. Copy the tested revision from the
run itself: a GitHub synthetic merge commit may differ from the PR head. Do not
relabel an old run to match a newer candidate, even when the code seems unchanged.
Run verification from the exact tested revision in a separate clean checkout when
retaining historical evidence.

Use `passed`, `failed`, `blocked` or `not-run`. A passed receipt requires at least one
nonempty artifact. Supported lanes are `hosted-ci`, `source-review`, `clean-host`,
`linux-offscreen`, `linux-wayland`, `qnx-cross-build`, `qnx-vm`, `qnx-physical`,
`marathon-shell` and `marathon-physical`. Physical lanes additionally require `board`
and `image` strings. Keep custom Pi400 and stock QSTI results separate.

A reviewer declaration is optional and must differ from the author when provided.
The tool cannot authenticate identities, approval or what actually happened on a
device. It never executes commands contained in receipts. Do not put credentials,
licensed SDK payloads, premium components or private device data in artifacts.

## Bind a boot or performance capture

Historical receipts without `evidence` stay valid but report
`verification: "integrity-only"` and `captureComplete: null`. They do not acquire
boot or performance completeness by having a `passed` outcome.

For typed receipts, the integrator names the candidate's intended target in the
dossier's top-level `releaseTarget`. Record all seven fields from the selected
candidate and target; do not infer a board from the `qnx-physical` lane:

```json
{
  "id": "YOUR_NAMED_RELEASE_TARGET",
  "lane": "qnx-physical",
  "board": "ACTUAL_BOARD_IDENTITY",
  "image": "ACTUAL_IMAGE_IDENTITY",
  "runtime": "ACTUAL_RUNTIME_IDENTITY",
  "binarySha256": "ACTUAL_BINARY_SHA256",
  "bundleSha256": "ACTUAL_HBC_SHA256"
}
```

This is a shape illustration, not evidence or a default target. The two hashes
must be full lowercase SHA-256 values. Keep a custom Pi400 target's ID, board and
image distinct from stock Pi4 QSTI. The runtime identity should identify the
selected Hermes/runtime build, not just say "Hermes".

Add `evidence` to the receipt with these fields:

| Field     | Value                                                                                              |
| --------- | -------------------------------------------------------------------------------------------------- |
| `kind`    | `cold-boot` or `performance`, equal to the receipt's `gate`                                        |
| `target`  | The actual run's identity, with the same shape as `releaseTarget`; use `null` if unknown           |
| `capture` | Path to the capture JSON, such as `artifacts/boot/capture.json`; use `null` when none was recorded |

Include the capture JSON in that receipt's `artifacts` with its actual byte count
and SHA-256. Keep the existing receipt commit, lockfile, lane, outcome, command,
environment, limitations, author and optional reviewer fields. Physical receipt
`board`/`image` must also match `evidence.target`. Do not invent author or reviewer
identities for an unexecuted run.

Use the existing [capture format](release-evidence.md). In its `provenance`,
record `targetId` equal to `evidence.target.id`, plus matching `sourceCommit`,
`lockfileSha256`, `lane`, `board`, `image`, `runtime`, `binarySha256` and
`bundleSha256`. Source and lock must equal the enclosing receipt and committed
candidate. The qualifier calls host lanes `host`; that maps to dossier lanes
`hosted-ci`, `source-review`, `clean-host`, `linux-offscreen` and `linux-wayland`.
Other lanes must match exactly. Host, VM and Marathon captures cannot become QNX
physical evidence through this mapping.
The qualifier has no cross-build or Marathon-shell capture lane; keep those
receipts integrity-only rather than relabelling them as physical runs.

Capture raw artifact paths resolve relative to the capture JSON's directory,
not the dossier. For example, `raw.log` in `artifacts/boot/capture.json` resolves
to `artifacts/boot/raw.log`. Every cold-boot attempt must reference a hashed raw
log. Do not include SDK payloads or native binaries merely to name their hashes.

Verification composes the existing capture qualifier and raw artifact checker.
It retains the qualifier's single 20-consecutive-final-power-cycle rule; reboots,
app relaunches and failed/blocked attempts reset that streak. Performance needs
all three workloads and presented-frame evidence; queued frames and JavaScript
callback cadence remain incomplete.

Missing `releaseTarget`, `target: null`, `capture: null`, a different named release
target, or an outcome other than `passed` produces `captureComplete: false` with
`blockers`. Failed, blocked and not-run receipts can have no artifacts when their
capture is null. A complete capture's provenance must match its receipt even if
its outcome is negative. Malformed captures, misbound identities, missing
referenced files and changed hashes are verification errors, not passes.

## Interpret the report

`integrityVerified` means the selected files match their recorded byte counts and
hashes and the receipt matches committed HEAD and its lockfile. `gateSnapshot` and
`openGates` reflect the repository's existing readiness record, not new acceptance
booleans. `releaseAuthorized` is always false, including when the source record has
no open gates. A host receipt attached to a cold-boot dossier remains host evidence;
it cannot satisfy or update the cold-boot gate.

For typed receipts, `verification: "candidate-capture-v1"` reports the bound
`target`, `capture`, `captureComplete` and `blockers`. `qualification` holds the
existing qualifier's structural result; it can be complete while the bound result
is incomplete (for example, a Pi400 capture for a stock Pi4 release target).
`artifactVerification` reports raw files checked. Both are null without a capture.
Neither structural nor bound completeness proves physical observations, a
performance budget, reviewer approval or release readiness. Target/runtime/binary
identities are declared correspondence checks, not build attestations.

Use independent review and `pnpm validate:v1` for the
[existing release process](../releases/v1-readiness.md). The standalone
[capture qualifier](../../scripts/qualify-qnx-evidence.mjs) remains available.
This tool does not enumerate required CI jobs or review artifact licensing.

## Resolve a verification error

A changed commit, tracked source, lockfile or gate record requires a new dossier;
do not edit candidate identities to disguise mismatched receipts. Changed or missing
artifacts need to be recovered from their actual run. Symlinks, hard-linked files,
parent-directory escapes and nonregular inputs are rejected. Each JSON is limited
to 1 MiB, each artifact to 128 MiB, and the aggregate declared artifact budget to
512 MiB, with at most 128 receipts and 16 artifacts per receipt.
Typed capture JSON is limited to 1 MiB, with 1–256 nonempty raw files of at most
64 MiB each. Raw files must also have one link and share the dossier's 512 MiB
budget; repeated references consume budget again. Symlink directories are rejected
for both the capture and its raw files.

The caller's repository, Git configuration and filesystem are trusted. These checks
are not a hostile-filesystem sandbox, a signature system or protection against an
author intentionally fabricating evidence. Only committed source is fingerprinted;
untracked or ignored files are not attested, and a clean snapshot does not prove
that any particular native build used it. Linux/Node tests do not qualify other
host operating systems. No boot, package version or target configuration changes.
