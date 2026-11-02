# November 2026 npm audit remediation — Expo SDK 58 / RN 0.88

Status: **security overrides and a real lockfile committed**. The one-shot regeneration job passed frozen install, source/JS checks, and the configured security audit on 2026-11-02 (GitHub Actions job 125509264858). Full final-commit CI and QNX/Marathon native runtime compatibility remain separate gates.

On 2026-11-02, PR #36's current frozen dependency graph completed install,
type checking and host C++ smoke tests, but `pnpm audit:dependencies` found
**14** unmitigated moderate-or-higher advisories (1 critical, 9 high,
4 moderate) across 7 package names. The code intentionally keeps the strict
audit gate. Do not add another advisory to `audit.ignore` or claim a test
passed merely because a version is patched upstream.

| CI affected package | CI version | Proposed version | Reason                                    |
| ------------------- | ---------- | ---------------- | ----------------------------------------- |
| `source-map-js`     | 1.2.1      | 1.2.2            | GHSA-68fv-2mgg-jv7q                       |
| `compression`       | 1.8.1      | 1.8.2            | GHSA-vc2v-76pw-4v95                       |
| `shell-quote`       | 1.9.x      | 1.12.0           | GHSA-pqg4-j6r4-53mv, critical             |
| `zod`               | 4.4.3      | 4.6.6            | GHSA-mrhx-c7xc-m3w5                       |
| `minimatch`         | 10.2.6     | 11.0.1           | GHSA-g3hv-75wg-m249                       |
| `glob`              | 13.0.6     | 14.0.3           | GHSA-3g3m-2fjr-6pw4                       |
| `node-forge`        | 1.4.0      | 1.4.2            | eight new Forge advisories, fixed >=1.4.1 |

The last row is supported by the published security release for
[node-forge 1.4.2](https://www.npmjs.com/package/node-forge).
`glob` / `minimatch` contain major version changes, so **do not**
treat a clean audit as sufficient compatibility proof. Check the Expo
CLI, Metro bundles, native QNX/Marathon host graph and file glob tests
independently. The selective `@expo/cli`, `react-devtools-core`, `nitrogen`, `glob`,
`@expo/fingerprint`, and `@ts-morph/common` parent overrides reduce unrelated changes.

## Required check sequence

```bash
pnpm install --lockfile-only
pnpm install --frozen-lockfile
node scripts/validate-security-overrides.mjs
pnpm validate:dependencies
pnpm test:js
pnpm typecheck
pnpm audit:dependencies
# Plus: SDK58 QNX production Metro bundle and the full Qt/Hermes host tests.
```

The one-time GitHub job regenerated the lockfile and checked frozen install,
source contracts and the audit. Its write-enabled workflow has been removed
from this PR after use; normal validation retains read-only token permissions.
Subsequent updates must rerun the real audit without advisory suppressions.

The source-verified patched `braces@3.0.3` remains a separate, explicitly
audited local backport in `scripts/lib/braces-security.mjs`.

## Revert

Revert the seven overrides and regenerate the prior lockfile as one change;
do not revert `pnpm-lock.yaml` independently of `pnpm-workspace.yaml`.
