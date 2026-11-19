#include "FilterColorSpace.h"

#include <array>
#include <cmath>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

struct Tables {
  std::array<quint8, 256> toLinear;
  std::array<quint8, 256> toSrgb;
  Tables() {
    for (int i = 0; i < 256; ++i) {
      const double c = i / 255.0;
      const double lin = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
      const double srgb = c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
      toLinear[static_cast<size_t>(i)] = static_cast<quint8>(std::lround(lin * 255.0));
      toSrgb[static_cast<size_t>(i)] = static_cast<quint8>(std::lround(srgb * 255.0));
    }
  }
};

const Tables& tables() {
  static const Tables t;
  return t;
}

}  // namespace

void convertColorSpace(QImage& image, ColorSpace from, ColorSpace to) {
  if (from == to || image.isNull()) return;
  const auto& lut = from == ColorSpace::SRGB ? tables().toLinear : tables().toSrgb;
  const int w = image.width();
  for (int y = 0; y < image.height(); ++y) {
    auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < w; ++x) {
      const QRgb p = line[x];
      const int a = qAlpha(p);
      if (a == 0) continue;
      const int r = lut[static_cast<size_t>(demul(qRed(p), a))];
      const int g = lut[static_cast<size_t>(demul(qGreen(p), a))];
      const int b = lut[static_cast<size_t>(demul(qBlue(p), a))];
      line[x] = qRgba(premul(r, a), premul(g, a), premul(b, a), a);
    }
  }
}

QImage inColorSpace(const QImage& image, ColorSpace from, ColorSpace to) {
  if (from == to || image.isNull()) return image;
  QImage copy = image.copy();
  convertColorSpace(copy, from, to);
  return copy;
}

QRgb premultipliedIn(const QColor& srgb, double opacity, ColorSpace space) {
  const double a = srgb.alphaF() * (opacity > 0 ? (opacity < 1 ? opacity : 1) : 0);
  const int ai = static_cast<int>(std::lround(a * 255.0));
  int r = srgb.red(), g = srgb.green(), b = srgb.blue();
  if (space == ColorSpace::LinearRGB) {
    r = tables().toLinear[static_cast<size_t>(r)];
    g = tables().toLinear[static_cast<size_t>(g)];
    b = tables().toLinear[static_cast<size_t>(b)];
  }
  return qRgba(premul(r, ai), premul(g, ai), premul(b, ai), ai);
}

}  // namespace rnqnx::views::rnsvg::filter::detail
