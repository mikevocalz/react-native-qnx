#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

// Porter-Duff on premultiplied pixels. `a` is the primary input and `b` the
// backdrop (in2).
template <class F>
void combine(QImage& out, const QImage& a, const QImage& b, F fn) {
  for (int y = 0; y < out.height(); ++y) {
    const auto* pa = reinterpret_cast<const QRgb*>(a.constScanLine(y));
    const auto* pb = reinterpret_cast<const QRgb*>(b.constScanLine(y));
    auto* po = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < out.width(); ++x) po[x] = fn(pa[x], pb[x]);
  }
}

QRgb channels(int r, int g, int b, int a) { return qRgba(r, g, b, a); }

QRgb scale(QRgb p, int k) {  // k in 0..255
  return channels(div255(qRed(p) * k), div255(qGreen(p) * k), div255(qBlue(p) * k), div255(qAlpha(p) * k));
}

QRgb add(QRgb p, QRgb q) {
  return channels(clampByte(qRed(p) + qRed(q)), clampByte(qGreen(p) + qGreen(q)), clampByte(qBlue(p) + qBlue(q)),
                  clampByte(qAlpha(p) + qAlpha(q)));
}

}  // namespace

Layer applyComposite(const Env&, const Composite& p, ColorSpace cs, const Layer& in, const Layer& in2) {
  const QImage a = inColorSpace(in.image, in.space, cs);
  const QImage b = inColorSpace(in2.image, in2.space, cs);
  QImage out(a.size(), QImage::Format_ARGB32_Premultiplied);
  const QString& op = p.op;
  if (op == QLatin1String("in")) {
    combine(out, a, b, [](QRgb s, QRgb d) { return scale(s, qAlpha(d)); });
  } else if (op == QLatin1String("out")) {
    combine(out, a, b, [](QRgb s, QRgb d) { return scale(s, 255 - qAlpha(d)); });
  } else if (op == QLatin1String("atop")) {
    combine(out, a, b, [](QRgb s, QRgb d) { return add(scale(s, qAlpha(d)), scale(d, 255 - qAlpha(s))); });
  } else if (op == QLatin1String("xor")) {
    combine(out, a, b, [](QRgb s, QRgb d) { return add(scale(s, 255 - qAlpha(d)), scale(d, 255 - qAlpha(s))); });
  } else if (op == QLatin1String("arithmetic")) {
    auto k = [](double v) { return std::isfinite(v) ? static_cast<float>(std::clamp(v, -1.0e6, 1.0e6)) : 0.0f; };
    const float k1 = k(p.k1), k2 = k(p.k2), k3 = k(p.k3), k4 = k(p.k4);
    const float inv = 1.0f / 255.0f;
    auto mix = [&](int s, int d) {
      const float sv = s * inv, dv = d * inv;
      const float v = k1 * sv * dv + k2 * sv + k3 * dv + k4;
      return v > 0 ? (v < 1 ? static_cast<int>(v * 255.0f + 0.5f) : 255) : 0;  // NaN gives 0
    };
    combine(out, a, b, [&](QRgb s, QRgb d) {
      const int ao = mix(qAlpha(s), qAlpha(d));
      // Premultiplied colors cannot exceed alpha.
      return channels(std::min(ao, mix(qRed(s), qRed(d))), std::min(ao, mix(qGreen(s), qGreen(d))),
                      std::min(ao, mix(qBlue(s), qBlue(d))), ao);
    });
  } else {  // over, and anything unrecognised
    combine(out, a, b, [](QRgb s, QRgb d) { return over(s, d); });
  }
  return {out, {}, cs};
}

}  // namespace rnqnx::views::rnsvg::filter::detail
