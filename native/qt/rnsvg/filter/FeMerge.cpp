#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

namespace rnqnx::views::rnsvg::filter::detail {

Layer applyMerge(const Env& env, ColorSpace cs, const std::vector<const Layer*>& nodes) {
  Layer out{makeImage(env.region.size()), {}, cs};
  for (const Layer* node : nodes) {
    const QImage src = inColorSpace(node->image, node->space, cs);
    for (int y = 0; y < out.image.height(); ++y) {
      const auto* s = reinterpret_cast<const QRgb*>(src.constScanLine(y));
      auto* d = reinterpret_cast<QRgb*>(out.image.scanLine(y));
      for (int x = 0; x < out.image.width(); ++x) d[x] = over(s[x], d[x]);
    }
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::filter::detail
