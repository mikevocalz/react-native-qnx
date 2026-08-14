# ADR-0007: Self-hosted QNX build infrastructure

## Status

Accepted

## Context

QNX SDP is licensed proprietary software and cannot be redistributed in public CI images. Cloud-hosted runners generally do not have QNX SDP installed.

## Decision

Use self-hosted CI runners or private agents that have licensed QNX SDP 8.0
installed. Record the exact installed update/build with the runner evidence.
Build scripts are idempotent and environment-variable driven so they can run in
any compliant private runner.

## Consequences

- Public CI cannot build QNX artifacts without a licensed self-hosted runner.
- Build reproducibility depends on pinned SDP versions and documented environment variables.
- QNX license material is never committed to the repository.

## Alternatives considered

- Public CI with downloaded QNX toolchain: rejected because it violates licensing and distribution terms.
- Prebuilt QNX images in public registry: rejected for the same reason.

## Related files

- `scripts/`
- `docs/getting-started/build-and-deploy.md`
- `docs/security/build-infra-security.md`

## Notes

CI configuration is intentionally kept generic; runner labels and secrets are injected by the operator.
