#pragma once

#include "Style.h"

#include <QImage>
#include <QPainterPath>
#include <QRect>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

class Scene;

// <clipPath> as geometry and <mask> as an alpha image.
class ClipMask {
 public:
  explicit ClipMask(Scene& scene) : scene_(scene) {}

  // The union of the clip path's children, in the user space of the element
  // that references it (the clipPath's own matrix applied). `elementRule` is
  // that element's clipRule: -1 unset, 0 evenodd, 1 nonzero; it is the fill
  // rule of children that do not name their own clip-rule.
  QPainterPath clipPath(const RnsvgElementItem& clip, int elementRule, const QSizeF& viewport, int depth);

  // The mask's content over `rect` (device pixels), as an image whose alpha
  // channel is the mask value: luminance times alpha by default, alpha alone
  // for mask-type alpha. `f` is the masked element's frame and `bbox` its
  // object bounding box in its own user space.
  QImage maskImage(const RnsvgElementItem& mask, const QRectF& bbox, const Frame& f, const QRect& rect, int depth);

 private:
  QPainterPath clipChild(const RnsvgElementItem* c, const Style& inherited, const Units& units, int elementRule,
                         int depth);
  Scene& scene_;
};

}  // namespace rnqnx::views::rnsvg
