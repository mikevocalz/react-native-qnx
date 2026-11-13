#pragma once

#include "Style.h"

#include <QRectF>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

class Scene;

// Element extents in the element's own user space (inside its own matrix).
class Bounds {
 public:
  explicit Bounds(Scene& scene) : scene_(scene) {}

  // The fill bounding box: what objectBoundingBox units measure. Geometry only,
  // no stroke, markers or filter regions.
  QRectF object(const RnsvgElementItem* e, const Frame& inherited, int depth);

  // Everything the element can paint: fill, stroke (with miter and cap
  // reach), markers, text and decorations, images, children, filter regions.
  // A box that contains the painted pixels, not the tightest one.
  QRectF paint(const RnsvgElementItem* e, const Frame& inherited, int depth);

 private:
  QRectF compute(const RnsvgElementItem* e, const Frame& inherited, int depth, bool painted);
  Scene& scene_;
};

}  // namespace rnqnx::views::rnsvg
