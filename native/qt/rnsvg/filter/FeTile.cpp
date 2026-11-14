#include "FilterPrimitives.h"

namespace rnqnx::views::rnsvg::filter::detail {

// The input's subregion is the tile. It repeats from its own top-left corner
// across the primitive's subregion and the layer keeps the input's space.
Layer applyTile(const Env& env, const QRect& sub, const Layer& in) {
  Layer out{makeImage(env.region.size()), {}, in.space};
  const QRect tile = in.subregion.intersected(in.image.rect());
  if (tile.isEmpty()) return out;
  const QRect r = sub.intersected(out.image.rect());
  auto wrap = [](int v, int origin, int period) {
    int m = (v - origin) % period;
    if (m < 0) m += period;
    return origin + m;
  };
  for (int y = r.top(); y <= r.bottom(); ++y) {
    const auto* s = reinterpret_cast<const QRgb*>(in.image.constScanLine(wrap(y, tile.top(), tile.height())));
    auto* d = reinterpret_cast<QRgb*>(out.image.scanLine(y));
    for (int x = r.left(); x <= r.right(); ++x) d[x] = s[wrap(x, tile.left(), tile.width())];
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::filter::detail
