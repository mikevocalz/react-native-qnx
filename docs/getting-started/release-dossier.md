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

## Add an observed receipt

The generated JSON records the exact HEAD commit, tree, lockfile, package manifest,
component matrix and eleven release gates. Its `receipts` array starts empty.
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

## Interpret the report

`integrityVerified` means the selected files match their recorded byte counts and
hashes and the receipt matches committed HEAD and its lockfile. `gateSnapshot` and
`openGates` reflect the repository's existing readiness record, not new acceptance
booleans. `releaseAuthorized` is always false, including when the source record has
no open gates. A host receipt attached to a cold-boot dossier remains host evidence;
it cannot satisfy or update the cold-boot gate.

Use the existing [capture qualifier](../../scripts/qualify-qnx-evidence.mjs) for
boot/performance capture structure, then independent review and
`pnpm validate:v1` for the [existing release process](../releases/v1-readiness.md).
This tool does not duplicate cold-boot counting, performance analysis, required-CI
job enumeration or artifact licensing review.

## Resolve a verification error

A changed commit, tracked source, lockfile or gate record requires a new dossier;
do not edit candidate identities to disguise mismatched receipts. Changed or missing
artifacts need to be recovered from their actual run. Symlinks, hard-linked files,
parent-directory escapes and nonregular inputs are rejected. Each JSON is limited
to 1 MiB, each artifact to 128 MiB, and the aggregate declared artifact budget to
512 MiB, with at most 128 receipts and 16 artifacts per receipt.

The caller's repository, Git configuration and filesystem are trusted. These checks
are not a hostile-filesystem sandbox, a signature system or protection against an
author intentionally fabricating evidence. Only committed source is fingerprinted;
untracked or ignored files are not attested, and a clean snapshot does not prove
that any particular native build used it. Linux/Node tests do not qualify other
host operating systems. No boot, package version or target configuration changes.
