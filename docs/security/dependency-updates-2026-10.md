# October 2026 dependency fixes

The 2026-10-13 audit of PR #32 found three unmitigated advisories. Scoped
overrides replace only the affected transitive packages with published fixes:

- `postcss>source-map-js`: 1.2.1 → 1.2.2, within PostCSS's `^1.2.1` range.
  [GHSA-68fv-2mgg-jv7q](https://github.com/advisories/GHSA-68fv-2mgg-jv7q)
- `@expo/cli>compression`: 1.8.1 → 1.8.2, within Expo CLI's `^1.7.4` range.
  [GHSA-vc2v-76pw-4v95](https://github.com/advisories/GHSA-vc2v-76pw-4v95)
- `react-devtools-core>shell-quote`: 1.10.0 → 1.11.0, within DevTools'
  `^1.6.1` range.
  [GHSA-pqg4-j6r4-53mv](https://github.com/advisories/GHSA-pqg4-j6r4-53mv)

Registry version/integrity metadata was checked before regenerating the lockfile
with Node 26.10.0 and pnpm 12.8.1. Direct SDK pins are unchanged. Remove an
override only when the owning dependency resolves a fixed version without it;
retain the behavioral tests and audit gate.

## Regression evidence

Each new test first failed with the affected installed dependency and passed
after its upgrade. `pnpm test:js` discovers all three:

- Shell quoting rejects four line terminators after comment tokens while
  preserving ordinary argument quoting. No generated command is executed.
- Source-map consumers reject oversized, nested and malformed section offsets;
  ordinary source positions and mappings still work. The failure case tests
  construction, avoiding costly serialization of the vulnerable map.
- Real loopback HTTP responses exercise gzip, deflate and Brotli. Complete
  responses decompress correctly; interrupted responses destroy their zlib
  streams. Requests and response-close waits are bounded and cleaned up.

## Audit and target boundaries

`pnpm audit:dependencies` passes with the unchanged guarded audit policy.
The raw registry output still includes the braces warning, which the existing
installed-source check classifies as locally mitigated. The existing node-forge
exception is unchanged. This is not a claim of a clean raw registry audit.
No new ignore entry or audit threshold change is made.

These fixes affect host development dependencies. They add no QNX cross-build,
Qt target, or physical Pi 4/5/400 evidence. Exact-commit CI is required before
calling the updated pull request green.
