#include "FilterPrimitives.h"

#include <algorithm>
#include <cstring>

namespace rnqnx::views::rnsvg::filter::detail {

// The offset is rounded to whole device pixels and keeps the input's color
// space, so no conversion error is added. A rotated or flipped transform
// moves the offset along the transformed vector.
Layer applyOffset(const Env& env, const Offset& p, const Layer& in) {
  const QPointF v = env.vectorToDevice(env.userLength(p.dx, Axis::X), env.userLength(p.dy, Axis::Y));
  const QSize size = in.image.size();
  Layer out{makeImage(size), {}, in.space};
  if (!std::isfinite(v.x()) || !std::isfinite(v.y())) return out;
  if (std::abs(v.x()) >= size.width() || std::abs(v.y()) >= size.height()) return out;
  const int dx = static_cast<int>(std::lround(v.x()));
  const int dy = static_cast<int>(std::lround(v.y()));
  const int x0 = std::max(0, dx), x1 = std::min(size.width(), size.width() + dx);
  if (x1 <= x0) return out;
  for (int y = std::max(0, dy); y < std::min(size.height(), size.height() + dy); ++y) {
    std::memcpy(out.image.scanLine(y) + x0 * 4, in.image.constScanLine(y - dy) + (x0 - dx) * 4,
                static_cast<size_t>(x1 - x0) * 4);
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::filter::detail
