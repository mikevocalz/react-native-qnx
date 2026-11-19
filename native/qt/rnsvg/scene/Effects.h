#pragma once

#include "Style.h"

#include <QRectF>
#include <functional>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

class Scene;

// The element's boxes in its own user space, computed only when an effect
// needs them (most elements have no mask, filter or layer).
struct EffectBoxes {
  std::function<QRectF()> objectBox;  // fill bounding box: units for gradients, masks and filters
  std::function<QRectF()> paintBox;   // everything the element paints, stroke included
};

// Filter, clip, mask and group opacity, in the order SVG applies them.
// Layers are as large as the element's painted extent (or its filter region),
// not the whole <Svg> texture.
class Effects {
 public:
  explicit Effects(Scene& scene) : scene_(scene) {}

  // Runs `draw` so that its output has the element's effects applied.
  // `layerForOpacity` is true when the element's opacity must apply to what it
  // draws as a unit (a group, or a shape with both fill and stroke).
  void paint(const RnsvgElementItem& e, const Frame& f, const EffectBoxes& boxes, bool layerForOpacity, int depth,
             const std::function<void()>& draw);

  // Hit testing: false when the point is outside the element's clip path or
  // where its mask is transparent, so nothing in the element can be hit.
  bool hitPasses(const RnsvgElementItem& e, const Frame& f, const EffectBoxes& boxes, int depth);

 private:
  Scene& scene_;
};

}  // namespace rnqnx::views::rnsvg
