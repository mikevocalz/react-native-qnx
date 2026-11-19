#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

int channelIndex(const QString& selector) {
  if (selector == QLatin1String("R")) return 0;
  if (selector == QLatin1String("G")) return 1;
  if (selector == QLatin1String("B")) return 2;
  return 3;
}

int channelOf(QRgb p, int index) {
  const int a = qAlpha(p);
  switch (index) {
    case 0: return demul(qRed(p), a);
    case 1: return demul(qGreen(p), a);
    case 2: return demul(qBlue(p), a);
    default: return a;
  }
}

}  // namespace

// P'(x,y) = P(x + scale * (XC(x,y) - 0.5), y + scale * (YC(x,y) - 0.5)) with
// the channels of in2 read non-premultiplied, in the primitive's color space.
// `in` is sampled with nearest-pixel lookup and not converted, so the result
// keeps its space. The displacement vector runs through the transform, so a
// rotated CTM displaces along the rotated axes. Samples outside the region are
// transparent.
Layer applyDisplacementMap(const Env& env, const DisplacementMap& p, ColorSpace cs, const QRect& sub, const Layer& in,
                           const Layer& in2) {
  Layer out{makeImage(env.region.size()), {}, in.space};
  if (!std::isfinite(p.scale)) return out;
  const QImage map = inColorSpace(in2.image, in2.space, cs);
  const int xc = channelIndex(p.xChannelSelector), yc = channelIndex(p.yChannelSelector);
  const double sx = env.userNumber(p.scale, Axis::X), sy = env.userNumber(p.scale, Axis::Y);
  const int w = out.image.width(), h = out.image.height();
  const QRect r = sub.intersected(out.image.rect());
  for (int y = r.top(); y <= r.bottom(); ++y) {
    const auto* m = reinterpret_cast<const QRgb*>(map.constScanLine(y));
    auto* d = reinterpret_cast<QRgb*>(out.image.scanLine(y));
    for (int x = r.left(); x <= r.right(); ++x) {
      const double ux = sx * (channelOf(m[x], xc) / 255.0 - 0.5);
      const double uy = sy * (channelOf(m[x], yc) / 255.0 - 0.5);
      const QPointF v = env.vectorToDevice(ux, uy);
      const double fx = std::floor(x + v.x() + 0.5), fy = std::floor(y + v.y() + 0.5);
      if (!(fx >= 0 && fx < w && fy >= 0 && fy < h)) continue;
      d[x] = reinterpret_cast<const QRgb*>(in.image.constScanLine(static_cast<int>(fy)))[static_cast<int>(fx)];
    }
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::filter::detail
