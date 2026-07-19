# ADR-0001: QNX Screen as the core UI platform

## Status

Accepted

## Context

QNX applications can use multiple graphics stacks. Qt is common in infotainment, but it is not universal in automotive, industrial, medical, or edge-device deployments. Treating Qt as a required dependency would limit the addressable target set and increase image size, licensing surface, and integration complexity.

## Decision

Use QNX Screen as the native display, compositor, window, buffer, input, and event integration layer. React Native surfaces are architected around QNX Screen's client/server model: one `screen_window_t` per top-level React Native surface, buffers rendered through EGL/OpenGL ES and presented with `screen_post_window()`.

## Consequences

- The platform runs on QNX targets that have Screen but no Qt.
- A future optional Qt interop package can still be built on top of the core platform.
- Input, lifecycle, and rendering semantics differ from iOS UIKit and Android; those differences are explicitly modeled in platform code.

## Alternatives considered

- Qt-based rendering: rejected because it would make Qt mandatory and does not map cleanly to Fabric's retained scene graph.
- SDL/Vulkan-only prototype: rejected because QNX Screen is the standard production path for windowing and composition.

## Related files

- `native/screen/`
- `native/application/main.cpp`
- `docs/architecture/qnx-screen-integration.md`

## Notes

Future Vulkan/Vulkan SC backends still present through QNX Screen windows.
