# ADR-0008: Version-pinning strategy

## Status

Accepted

## Context

React Native, Expo, Hermes, Skia, and QNX SDP move at different cadences. Floating versions cause non-reproducible builds and hard-to-diagnose integration failures.

## Decision

Pin exact versions for every dependency in the compatibility matrix, lockfiles, and CMake dependency manifests. Updates require an ADR or decision-log entry, a matrix update, a lockfile update, and revalidation of the affected targets.

## Consequences

- Reproducible builds across hosts.
- Clear upgrade path documented in ADRs.
- No silent major-version drift.

## Alternatives considered

- Semver ranges: rejected because they allow drift that can break cross-compilation and native linking.
- Latest-of-everything: rejected because the newest package is not always compatible with the selected React Native and Expo releases.

## Related files

- `docs/getting-started/compatibility-matrix.md`
- `pnpm-lock.yaml`
- `cmake/dependencies/`

## Notes

"Verify" entries in the matrix are temporary and must be resolved to a pin before Milestone 0 exits.
