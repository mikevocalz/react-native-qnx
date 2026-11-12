#pragma once

#include <QMetaType>
#include <QSize>
#include <QtGlobal>

namespace rnqnx::graphics {

// Qt's selected backend is not evidence of an installed external engine or of
// a device feature such as WebGPU, compute shaders, or texture interoperability.
enum class GraphicsBackend {
  Unknown, Software, OpenVG, OpenGL, Direct3D11, Vulkan, Metal, Null, Direct3D12
};

enum class GraphicsSurfaceState {
  Uninitialized, Initialized, Invalidated, Destroyed, Closed
};

struct GraphicsViewport {
  QSize logicalSize{0, 0};
  QSize pixelSize{0, 0};
  qreal devicePixelRatio{1.0};

  // Round each physical dimension to the nearest integer, as QSize * DPR
  // does. Reject negative dimensions, non-positive/non-finite DPR and overflow.
  static GraphicsViewport fromLogicalSize(QSize logicalSize, qreal devicePixelRatio);
};

// A value snapshot. No field owns a Qt window, graphics context or GPU handle.
// Generation changes on viewport/storage or scene-graph lifecycle transitions.
// Sequence additionally changes when visibility/exposure changes.
struct GraphicsSurfaceSnapshot {
  quint32 schemaVersion{1};
  GraphicsSurfaceState state{GraphicsSurfaceState::Uninitialized};
  GraphicsBackend backend{GraphicsBackend::Unknown};
  GraphicsViewport viewport;
  quint64 generation{0};
  quint64 sequence{0};
  bool isVisible{false};
  bool isExposed{false};
};

struct GraphicsFrameSwap {
  quint32 schemaVersion{1};
  quint64 sequence{0};
  // Observation generation at GUI delivery, not a GPU resource lease.
  quint64 generation{0};
  // Monotonic time since observation construction, sampled on GUI delivery.
  // This is neither the render-thread signal time nor a scanout timestamp.
  qint64 observedAtNanoseconds{0};
};

}  // namespace rnqnx::graphics

Q_DECLARE_METATYPE(rnqnx::graphics::GraphicsSurfaceSnapshot)
Q_DECLARE_METATYPE(rnqnx::graphics::GraphicsFrameSwap)
