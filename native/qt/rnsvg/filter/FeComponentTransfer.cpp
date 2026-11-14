#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

#include <array>
#include <cmath>
#include <optional>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

using Lut = std::array<quint8, 256>;

double finiteOr(double v, double fallback) { return std::isfinite(v) ? v : fallback; }

// nullopt for the identity, which needs no pass over the pixels.
std::optional<Lut> buildLut(const TransferFunction& f) {
  Lut lut;
  auto fill = [&](auto&& fn) {
    for (int i = 0; i < 256; ++i) {
      lut[static_cast<size_t>(i)] = static_cast<quint8>(std::lround(clamp01(fn(i / 255.0)) * 255.0));
    }
  };
  if (f.type == QLatin1String("table")) {
    const auto& v = f.tableValues;
    if (v.empty()) return std::nullopt;
    const size_t n = v.size() - 1;  // intervals
    fill([&](double c) {
      if (n == 0) return finiteOr(v[0], 0);
      size_t k = static_cast<size_t>(c * static_cast<double>(n));
      if (k >= n) return finiteOr(v[n], 0);
      const double lo = finiteOr(v[k], 0), hi = finiteOr(v[k + 1], 0);
      return lo + (c - static_cast<double>(k) / static_cast<double>(n)) * static_cast<double>(n) * (hi - lo);
    });
    return lut;
  }
  if (f.type == QLatin1String("discrete")) {
    const auto& v = f.tableValues;
    if (v.empty()) return std::nullopt;
    const size_t n = v.size();
    fill([&](double c) {
      size_t k = static_cast<size_t>(c * static_cast<double>(n));
      if (k >= n) k = n - 1;
      return finiteOr(v[k], 0);
    });
    return lut;
  }
  if (f.type == QLatin1String("linear")) {
    const double slope = finiteOr(f.slope, 1), intercept = finiteOr(f.intercept, 0);
    fill([&](double c) { return slope * c + intercept; });
    return lut;
  }
  if (f.type == QLatin1String("gamma")) {
    const double amp = finiteOr(f.amplitude, 1), exp = finiteOr(f.exponent, 1), off = finiteOr(f.offset, 0);
    fill([&](double c) { return amp * std::pow(c, exp) + off; });
    return lut;
  }
  return std::nullopt;
}

}  // namespace

Layer applyComponentTransfer(const Env&, const ComponentTransfer& p, ColorSpace cs, const Layer& in) {
  const auto lr = buildLut(p.r), lg = buildLut(p.g), lb = buildLut(p.b), la = buildLut(p.a);
  if (!lr && !lg && !lb && !la) return in;
  QImage image = inColorSpace(in.image, in.space, cs).copy();
  for (int y = 0; y < image.height(); ++y) {
    auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      const QRgb px = line[x];
      const int a = qAlpha(px);
      int r = demul(qRed(px), a), g = demul(qGreen(px), a), b = demul(qBlue(px), a);
      if (lr) r = (*lr)[static_cast<size_t>(r)];
      if (lg) g = (*lg)[static_cast<size_t>(g)];
      if (lb) b = (*lb)[static_cast<size_t>(b)];
      const int ao = la ? (*la)[static_cast<size_t>(a)] : a;
      line[x] = qRgba(premul(r, ao), premul(g, ao), premul(b, ao), ao);
    }
  }
  return {image, {}, cs};
}

}  // namespace rnqnx::views::rnsvg::filter::detail
