# ADR-0009: Fabric-only renderer architecture

## Status

Accepted

## Context

Supporting both Paper and Fabric would duplicate the native surface and add a
legacy bridge that is outside the project charter.

## Decision

Implement only Fabric plus JSI. There is no Paper view-manager registration or
legacy application bridge. Application-facing native APIs use Nitro
HybridObjects; React Native internal host contracts are implemented only where
installed source proves they are required.

## Consequences

- The platform follows React Native's New Architecture.
- The QNX mounting layer must implement the installed Fabric scheduler,
  transaction, mutation, event, and component-descriptor contracts.
- A React Native internal interface mentioning TurboModules does not authorize
  TurboModules as the QNX application module system.

## Alternatives considered

- **Paper plus Fabric:** rejected because it doubles unproven platform seams.
- **Legacy bridge first:** rejected by the project charter.

## Evidence boundary

Installed React Native 0.86.2 currently provides the host contracts named in
`docs/handoff/CURRENT_STATE.md`. No headless Fabric QNX build has passed yet; P2
remains behind G0 and P1.
