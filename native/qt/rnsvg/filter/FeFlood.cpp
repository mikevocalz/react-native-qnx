#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

namespace rnqnx::views::rnsvg::filter::detail {

QColor brushColor(const Brush& brush, const Env& env, const QColor& fallback) {
  switch (brush.type) {
    case Brush::Type::Color: return brush.color;
    case Brush::Type::CurrentColor: return env.context->currentColor;
    default: return fallback;
  }
}

// The flood color is an sRGB value, so the layer is tagged sRGB and a later
// consumer converts it if it works in linearRGB.
Layer applyFlood(const Env& env, const Flood& p, const QRect& sub) {
  Layer out{makeImage(env.region.size()), {}, ColorSpace::SRGB};
  const QColor color = brushColor(p.color, env, QColor(Qt::transparent));
  const QRgb px = premultipliedIn(color, p.opacity, ColorSpace::SRGB);
  const QRect r = sub.intersected(out.image.rect());
  for (int y = r.top(); y <= r.bottom(); ++y) {
    auto* line = reinterpret_cast<QRgb*>(out.image.scanLine(y));
    std::fill(line + r.left(), line + r.right() + 1, px);
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::filter::detail
