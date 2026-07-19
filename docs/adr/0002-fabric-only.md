# ADR-0002: Fabric-only renderer architecture

## Status

Accepted

## Context

React Native supports the legacy Paper renderer and the New Architecture (Fabric + TurboModules). Building both would double the native surface area and require legacy bridge infrastructure that is being removed from upstream.

## Decision

Implement only the New Architecture. The Fabric renderer is the sole supported renderer. There is no legacy bridge, no Paper view manager registration, and no synchronous legacy native module path.

## Consequences

- Smaller native runtime.
- Alignment with upstream React Native direction.
- All native application APIs use Nitro Modules.

## Alternatives considered

- Paper + Fabric hybrid: rejected because it adds maintenance burden for a green-field platform.

## Related files

- `native/fabric/`
- `native/renderer/`

## Notes

A small internal compatibility adapter may satisfy React Native core interfaces that still reference the TurboModule registry; the adapter delegates to Nitro or the platform runtime.
