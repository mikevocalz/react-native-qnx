#pragma once

#include "Style.h"

#include <QBrush>
#include <QRectF>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

class Scene;

// A resolved paint: a Qt brush plus the opacity to draw it with. Colors and
// gradients carry their opacity inside the brush (opacity stays 1); a pattern
// tile cannot, so its opacity goes to the painter.
struct PaintBrush {
  QBrush brush;
  double opacity{1};
};

// Turns an element's fill or stroke (`Brush`) into something QPainter can draw
// with: flat colors, linear and radial gradients with their units, transform
// and spread, and patterns.
class PaintServers {
 public:
  explicit PaintServers(Scene& scene) : scene_(scene) {}

  // False when the brush paints nothing: `none`, a missing or empty server, a
  // degenerate bounding box for objectBoundingBox units.
  bool make(const Brush& b, const QRectF& bbox, const Frame& f, double opacity, PaintBrush& out);

 private:
  bool gradient(const ElementProps& g, const QRectF& bbox, const Frame& f, double opacity, PaintBrush& out);
  bool pattern(const RnsvgElementItem& server, const QRectF& bbox, const Frame& f, double opacity,
               PaintBrush& out);
  Scene& scene_;
  int patternDepth_{0};
};

}  // namespace rnqnx::views::rnsvg
