# ADR-0005: Hermes as the initial JavaScript engine

## Status

Accepted

## Context

React Native supports multiple JavaScript engines. Hermes is the default and best-supported engine for React Native on production platforms, with deterministic bytecode, small bundles, and direct React Native integration.

## Decision

Use Hermes as the initial and required JavaScript engine. Hermes is cross-compiled for QNX. The build system contains a configure-time check for all required Hermes artifacts.

## Consequences

- Consistent bytecode behavior across host and target.
- Inspector and Fast Refresh integration follow Hermes paths.
- A different engine could be added later as an optional package.

## Alternatives considered

- JavaScriptCore: rejected because RN's JSC integration is less maintained on new platforms.
- V8: rejected because V8 is large and RN integration is not upstream.
- QuickJS: rejected because it does not support the full JSI surface needed by RN.

## Related files

- `native/hermes/`
- `cmake/dependencies/Hermes.cmake`

## Notes

If Hermes cannot be built, the configure step must fail with the exact missing component; no stub engine is allowed.
