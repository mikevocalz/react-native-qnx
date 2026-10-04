# braces depth-exhaustion backport

## Status and source

On 2026-10-03 the npm registry still published `braces@3.0.3` as latest, and
[GHSA-vfj7-8cjw-p6xm](https://github.com/advisories/GHSA-vfj7-8cjw-p6xm)
listed all versions through 3.0.3 as affected, without a patched release.
`micromatch@4.0.8` brings it into the Metro/Expo tooling graph. The existing
`brace-expansion` override addresses a different package.

`patches/braces@3.0.3.patch` backports only the five changed library files from
[upstream PR #72](https://github.com/micromatch/braces/pull/72), pinned to
[28d440b5dd449dbf1fe6f3506cf94ecca4d02660](https://github.com/micromatch/braces/commit/28d440b5dd449dbf1fe6f3506cf94ecca4d02660).
That PR remains unmerged. The patch applies to the published 3.0.3 source; it
does not substitute unreleased upstream parser changes or alter package identity.
The package's MIT license is retained. pnpm records the patch hash in the frozen
lockfile and applies it before use.

## Protection and compatibility

- Parsing limits combined brace/parenthesis nesting to 100. A smaller
  `maxDepth` is honored, including fractional limits; larger/non-finite limits
  cannot remove the cap.
- Recursive compile, expand, and stringify traversal also checks depth for
  caller-supplied ASTs. Expansion detects cyclic parent chains.
- Patterns exceeding the cap now throw intentionally. Ordinary globs, ranges,
  escaped/malformed shallow patterns, and `escapeInvalid` retain tested behavior.
- This is a depth-specific fix, not a total expansion-size bound or validation
  of every arbitrary malformed AST shape. It makes no QNX target-runtime claim.

## Audit gate

A version-based registry audit still reports 3.0.3. `pnpm audit:dependencies`
therefore runs `scripts/audit-dependencies.mjs`, which:

1. Inventories every reachable installed `braces` copy across all workspaces.
2. Requires version 3.0.3 and exact SHA-256 hashes for the package manifest,
   entrypoint, and every library file, then executes depth and compatibility
   regressions against each installed copy.
3. Runs the registry audit at the existing moderate threshold and prints its
   complete JSON report. The existing unrelated node-forge exception is unchanged.
4. Recognizes only the exact GHSA, package, affected range, severity, and 3.0.3
   findings as **locally mitigated**, after verification succeeds. It fails on
   other moderate-or-higher advisories, missing/changed code or versions, malformed
   audit data, execution failures, or an advisory that now identifies a fixed release.

The braces GHSA is deliberately absent from `audit.ignore`. Raw `pnpm audit
--audit-level moderate` remains nonzero while the registry finding exists; a
passing guarded gate must not be described as a clean registry audit. Hashes
prove the reviewed backport is installed, not that the dependency has no other
vulnerabilities. CI uses a frozen install before the gate.

## Verification

Run `pnpm install --frozen-lockfile`, `pnpm test:js`, `pnpm typecheck`, and
`pnpm audit:dependencies`. The JavaScript suite tests source tampering, absent
or changed versions, changed advisory scope, new moderate/high/critical findings,
and malformed reports as well as the installed patch's behavior.

Independent backport review on Node 24.19.0 passed the published package's
764 tests, all ten upstream security regression additions (774 total), and
48,277 differential/boundary assertions. The original 9,997-character nested
brace input reproduced stack exhaustion; the patched implementation rejects
at depth 101. QNX workspace validation uses Node 26.10.0 and pnpm 12.8.1;
current command results are recorded in `docs/handoff/TEST_STATUS.md`.

## Removal

When an official fixed release is available, verify registry/advisory metadata,
replace the backport with the smallest compatible dependency update, regenerate
the lockfile, and rerun the full SDK 58 validation including production Metro
bundling. Remove the braces patch, `patchedDependencies` entry, temporary hash
attestation and mitigation classification together; restore the direct registry
audit command. Keep the regression coverage adapted to the new release. Do not
rename the package or invent a higher version to hide the advisory.
