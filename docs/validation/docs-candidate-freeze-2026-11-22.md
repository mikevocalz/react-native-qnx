# Frozen candidate documentation, 2026-11-22

Base: `20a631fd0001fb02e75b9b66014371ea5c9b3da5`. Role: documentation and
developer-experience agent. This change adds an isolated build/export command;
it does not publish documentation, create a release tag or change a gate status.

## Implemented

`docs-site/scripts/freeze-candidate.mjs` resolves a committed revision, clones
it separately, installs the site's locked dependencies, checks generated pages,
builds at `/react-native-qnx/v/candidate-<commit>/` and checks the export. It
refuses an existing output or lock and inventories exported files by SHA-256.
The work directory and npm cache can be put on an external volume. A failed
build removes temporary state; an interrupted build's lock needs inspection.

The continuation also regenerates frozen source links using
`DOCS_SOURCE_COMMIT`. The normal site's generated content remains unchanged.
Release notes now allow reviewed, published candidate documentation to satisfy
the versioning part of the documentation gate before the final v1 release tag.
The documentation gate remains pending until publication and independent review.

## Host checks

Host: macOS, Node 26.13.0, npm 12.2.1. No QNX toolchain, simulator, device or
native runtime was used.

| Check | Actual result |
| --- | --- |
| `node tests/js/docs-candidate-snapshot.test.mjs` | Pass: real clone/checkout/npm pipeline on a tiny synthetic site; committed-source isolation, frozen path and hash receipt, preserved existing output, refused dangling symlink and unsafe path, failed-build cleanup |
| Root TypeScript `tsc --noEmit` | Pass after typed fixture helpers and UTF-8 JSON parsing |
| `node docs-site/scripts/generate-data.mjs --check` | Pass with default live-site settings |
| `node docs-site/scripts/generate-api.mjs --check` | Pass; 226 headers, 1904 declarations, none unparsed |
| `node docs-site/scripts/check-content.mjs` | Pass: 164 pages |
| `node scripts/validate-docs.mjs` | Pass before dependency-directory relocation |
| Pinned Prettier on changed docs/code | Pass before dependency-directory relocation; rerun on final integrated head |

An actual isolated Next.js build of the base revision completed on the external
volume. `npm ci --no-audit --no-fund`, `npm run data:check`, `npm run build` and
`npm run check:content` exited zero. The snapshot has **1504 exported files**;
all 1504 file hashes were independently read back and verified.

- Source commit: `20a631fd0001fb02e75b9b66014371ea5c9b3da5`.
- Frozen path: `/react-native-qnx/v/candidate-20a631fd0001`.
- Site lock SHA-256: `64eebe8cf35886af9fb687ab25b1bf8d801e0a50c6621cd34eedc603db98b0f7`.
- Snapshot receipt SHA-256: `46e32f4beeb3b393f0d4d15acb88e0066fb1e223c572fe2e2c60e454b28363fa`.

That build began before the source-link regeneration continuation was added.
It proves the base site exports at a frozen path and is inventoried correctly;
the final committed continuation must receive its own build, including the new
source-link regeneration step. The artifact is local and unpublished. No raw
Next build log was retained; the command exit results and hash readback are
recorded here without claiming a hosted build or signed provenance.

The first internal-disk attempt was interrupted when the parent identified the
disk floor. Its owned checkout, cache and lock were removed. The successful
attempt kept its work, dependencies, cache and output on the external volume.

## Skill and review boundary

Applied repository `qnx-platform-validation` from the base: documentation-only
checks stay separate from target acceptance. Read Callstack
`writing-user-docs` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`,
`skills/writing-user-docs/SKILL.md`, in full through GitHub. Applied its concrete
prerequisites, copyable command and failure-recovery guidance to the README.
No skill installation, native/API work or CI workflow change is claimed.
Independent implementation review remains required before merging.

## Remaining acceptance

Review the final candidate and its hashes, publish it at the exact frozen path,
then add it to `versions.json`. Publish the final v1 tag after the release
criteria pass. The developer-path gate still needs a new licensed developer's
end-to-end build/deploy/run evidence. Existing hosted clean-app and external
consumer checks are useful host prerequisites, not that target acceptance.

Rollback: revert this documentation/tooling continuation. Preserve any already
published immutable snapshots and their receipts.
