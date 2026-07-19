# ADR-0003: Skia as the initial graphics renderer

## Status

Accepted

## Context

A production renderer must draw rectangles, rounded rectangles, paths, borders, gradients, images, text, clipping, transforms, opacity layers, and shadows. Rewriting a full GPU renderer from scratch is not feasible for the initial release.

## Decision

Use Skia as the initial production renderer, connected to QNX Screen through EGL/OpenGL ES or the strongest validated graphics path on the target BSP. The graphics backend is abstracted (`GraphicsBackend`, `RenderSurface`, `RenderContext`, `FrameScheduler`) so that Vulkan or Vulkan SC can be introduced without rewriting Fabric integration.

## Consequences

- Faster path to production rendering.
- Skia's Paragraph module is reused for complex text layout.
- Backend capability reporting is required so unsupported features do not silently degrade.

## Alternatives considered

- Custom 2D renderer: rejected due to scope.
- Direct OpenGL ES 2D renderer: rejected because it does not solve text/image decode complexity.

## Related files

- `native/renderer/`
- `native/text/`
- `native/images/`

## Notes

Skia must be cross-compiled for QNX. The exact Skia commit is recorded in the compatibility matrix.
