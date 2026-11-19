#pragma once

#include "Effects.h"
#include "Style.h"

#include <QImage>
#include <QPoint>
#include <QRect>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

class Scene;

// Adapts a <filter> element and its primitive children to filter/FilterEngine.h
// for one filtered element.
class FilterRunner {
 public:
  explicit FilterRunner(Scene& scene) : scene_(scene) {}

  // The filter region in device pixels, where the element must be rendered.
  QRect region(const RnsvgElementItem& filter, const ElementProps& target, const Frame& f, const EffectBoxes& boxes);

  // The filter region in the element's user space, for layer sizing of an
  // ancestor. `objectBox` is the element's fill bounding box.
  QRectF regionInUserSpace(const RnsvgElementItem& filter, const Frame& f, const QRectF& objectBox);

  // Replaces `image` (the element rendered over `rect`) with the filtered
  // result and sets `origin` to its device position.
  void run(const RnsvgElementItem& filter, const ElementProps& target, const Frame& f, const EffectBoxes& boxes,
           const QRect& rect, QImage& image, QPoint& origin);

 private:
  Scene& scene_;
};

}  // namespace rnqnx::views::rnsvg
