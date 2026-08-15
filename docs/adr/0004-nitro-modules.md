# ADR-0004: Nitro Modules for native application APIs

## Status

Accepted

## Context

React Native historically used TurboModules for native module authoring. TurboModules require manual C++ boilerplate and are not type-safe across the JavaScript/native boundary. Nitro Modules provides generated JSI bindings and C++ HybridObjects with typed contracts.

## Decision

All new QNX application-facing native APIs use Nitro Modules. Nitrogen-generated code and C++ HybridObjects form the standard bridge between TypeScript and QNX native services. A minimal internal adapter satisfies React Native core interfaces that expect TurboModule registry semantics, but the adapter delegates real behavior to Nitro or the platform runtime.

## Consequences

- Type-safe native APIs from TypeScript definitions.
- Consistent ownership and lifetime semantics for HybridObjects.
- Expo Modules Core integration is built on top of Nitro primitives.

## Alternatives considered

- TurboModules-first: rejected because it conflicts with the goal of type-safe, modern native APIs and increases boilerplate.

## Related files

- `packages/qnx-nitro-runtime/`
- `native/nitro/`
- `docs/native-modules/nitro-guide.md`

## Notes

Do not generate empty TurboModules merely to silence linker errors.

Where the boundary falls, confirmed by the project owner on 2026-08-14: modules
React Native's own JavaScript requires by name are implemented in C++ against
React Native's generated `…CxxSpec` classes, because `TurboModuleRegistry` reads
a global that Nitro's registry does not populate and those call sites are inside
React Native's source. Everything an application calls is a Nitro HybridObject.
See `docs/handoff/DECISIONS.md`.
