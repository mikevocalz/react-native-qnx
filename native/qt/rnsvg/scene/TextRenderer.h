#pragma once

#include "Style.h"

#include <QPainterPath>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

class Scene;

// <Text>, <TSpan> and <TextPath>: builds the text layout input from the
// element subtree, asks the layout engine for glyph outlines, and fills and
// strokes each node's outlines with that node's own paint.
class TextRenderer {
 public:
  explicit TextRenderer(Scene& scene) : scene_(scene) {}

  // Paints (or hit-tests) the <Text> element `e` whose own frame is `f`.
  void draw(const RnsvgElementItem& e, const Frame& f, int depth);

  // All glyph outlines of the text subtree in the element's own space, used for
  // bounding boxes and for text inside a <clipPath>. `f` is the element's frame
  // including its own props.
  QPainterPath outline(const RnsvgElementItem* e, const Frame& f, int depth);

  // The union of the em boxes of the characters: the text's object bounding
  // box for gradients, masks and filters (SVG measures text by its cells, not
  // by its ink).
  QRectF cellBox(const RnsvgElementItem* e, const Frame& f, int depth);

 private:
  Scene& scene_;
};

}  // namespace rnqnx::views::rnsvg
