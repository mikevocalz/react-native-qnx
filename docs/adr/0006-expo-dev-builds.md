# ADR-0006: Expo development builds instead of Expo Go

## Status

Accepted

## Context

Expo Go provides a generic runtime for development, but it cannot include QNX-specific native code, QNX Screen integration, or proprietary BSP libraries. A custom development build is required for any QNX-specific native module or renderer.

## Decision

Target Expo development builds, not Expo Go. The `expo-qnx` platform layer implements Continuous Native Generation (CNG), prebuild, `run:qnx`, and `export --platform qnx` workflows.

## Consequences

- Application developers build a QNX-specific development client.
- Fast Refresh, bundle reload, and the developer menu are provided by the QNX development client.
- Expo Go is not supported and never will be for this platform.

## Alternatives considered

- Expo Go adaptation: rejected because QNX native code cannot ship inside the generic Expo Go binary.

## Related files

- `packages/expo-qnx/`
- `template/qnx/`

## Notes

Development builds reuse the same QNX CMake build path as production builds.
