#pragma once

#include "RnsvgItems.h"

namespace rnqnx::views::rnsvg {

// Paints the root's retained element tree. Call only where the QQuickPaintedItem
// contract allows reading item state: the Qt GUI thread, or the render thread
// while the GUI thread is blocked in synchronization. Returns what the paint
// allocated.
PaintStats paintScene(const RnsvgRootItem& root, QPainter* painter);

// Topmost responsible element at `local` (root item coordinates), or nullptr.
const RnsvgElementItem* hitScene(const RnsvgRootItem& root, const QPointF& local);

// The viewBox/preserveAspectRatio transform from user space to item space.
QTransform viewTransform(const RootProps& props, const QSizeF& itemSize);

}  // namespace rnqnx::views::rnsvg
