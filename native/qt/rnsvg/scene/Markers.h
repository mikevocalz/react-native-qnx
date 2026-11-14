#pragma once

#include "Style.h"

#include <QPainterPath>
#include <QRectF>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

class Scene;

// marker-start, marker-mid and marker-end on a Path or Line.
class Markers {
 public:
  explicit Markers(Scene& scene) : scene_(scene) {}

  // Draws the markers `f.style` names at the element's vertices. `bbox` is the
  // element's fill box, for context-fill and context-stroke inside a marker.
  void draw(const RnsvgElementItem& e, const Frame& f, const QRectF& bbox, int depth);

  // The user-space box the markers can paint in (the marker viewports at every
  // vertex), for layer sizing. Null when there are none.
  QRectF bounds(const RnsvgElementItem* e, const Frame& f, int depth);

 private:
  Scene& scene_;
};

}  // namespace rnqnx::views::rnsvg
