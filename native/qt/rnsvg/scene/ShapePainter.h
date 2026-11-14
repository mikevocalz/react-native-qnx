#pragma once

#include "Style.h"

#include <QPainterPath>
#include <QPen>

namespace rnqnx::views::rnsvg {

class Scene;

// Fills and strokes one outline with the paint of its Style.
class ShapePainter {
 public:
  explicit ShapePainter(Scene& scene) : scene_(scene) {}

  // `bbox` is the fill bounding box in the outline's own space, used by
  // objectBoundingBox paint servers.
  void draw(const QPainterPath& path, const Frame& f, const QRectF& bbox);

  // The stroke width in user units (0 when the stroke is off).
  static double strokeWidth(const Style& st, const Units& u);
  // The pen for `brush` with the Style's width, caps, joins, miter limit and
  // dashes. `itemScale` is the scale from the element's user space to item space,
  // which a non-scaling stroke divides back out.
  static QPen pen(const QBrush& brush, const Style& st, const Units& u, double itemScale);
  static Qt::PenCapStyle capStyle(int cap);
  static Qt::PenJoinStyle joinStyle(int join);

 private:
  Scene& scene_;
};

}  // namespace rnqnx::views::rnsvg
