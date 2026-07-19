# DECISIONS.md

## Decision log

Major architectural decisions are recorded as ADRs in `docs/adr/`. This file captures smaller or emerging decisions that do not yet warrant a full ADR.

### 2026-07-19 – Repository structure follows React Native out-of-tree platform conventions

- **Decision:** Adopt a monorepo with `apps/`, `packages/`, `native/`, `cmake/`, `template/`, `scripts/`, `tests/`, and `docs/`.
- **Reason:** Aligns with `react-native-linux` structural inspiration and Expo platform support expectations, while keeping QNX-specific code in clearly owned directories.
- **Alternatives considered:** Separate repos for runtime, Expo, and examples; rejected because cross-cutting changes (toolchain, Fabric, Nitro) require atomic commits.
- **Consequences:** Larger repository, but simpler coordinated releases.
- **Related files:** Repository root layout.
- **Should become an ADR:** Yes, once Milestone 0 exits.
