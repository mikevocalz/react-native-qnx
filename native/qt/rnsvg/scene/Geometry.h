#pragma once

#include "Style.h"

#include <QPainterPath>
#include <vector>

namespace rnqnx::views::rnsvg {

// Path, Rect, Circle, Ellipse and Line: the elements that are one outline.
bool isShape(Kind k);

// The outline of a basic shape in its own user space; empty for a degenerate
// one (a zero radius, a non-positive size).
QPainterPath shapePath(const ElementProps& p, const Units& units);

// Marker vertices of a shape. Only Path and Line carry markers; everything
// else yields none.
std::vector<PathVertex> shapeVertices(const ElementProps& p, const Units& units);

// Maps `viewBox` into a viewport of `size` (SVG preserveAspectRatio). Returns
// the identity when the viewBox is unset or degenerate.
QTransform viewBoxTransform(const ViewBox& box, const QSizeF& size);

}  // namespace rnqnx::views::rnsvg
