#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

using Matrix = std::array<double, 20>;  // 4 rows of R G B A bias

constexpr Matrix kIdentity{1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};

Matrix saturate(double s) {
  return {0.213 + 0.787 * s, 0.715 - 0.715 * s, 0.072 - 0.072 * s, 0, 0,
          0.213 - 0.213 * s, 0.715 + 0.285 * s, 0.072 - 0.072 * s, 0, 0,
          0.213 - 0.213 * s, 0.715 - 0.715 * s, 0.072 + 0.928 * s, 0, 0,
          0, 0, 0, 1, 0};
}

Matrix hueRotate(double degrees) {
  const double a = degrees * kPi / 180.0;
  const double c = std::cos(a), s = std::sin(a);
  return {0.213 + c * 0.787 - s * 0.213, 0.715 - c * 0.715 - s * 0.715, 0.072 - c * 0.072 + s * 0.928, 0, 0,
          0.213 - c * 0.213 + s * 0.143, 0.715 + c * 0.285 + s * 0.140, 0.072 - c * 0.072 - s * 0.283, 0, 0,
          0.213 - c * 0.213 - s * 0.787, 0.715 - c * 0.715 + s * 0.715, 0.072 + c * 0.928 + s * 0.072, 0, 0,
          0, 0, 0, 1, 0};
}

constexpr Matrix kLuminanceToAlpha{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2125, 0.7154, 0.0721, 0, 0};

// A wrong number of values for the type means the identity matrix, as in
// resvg. NaN entries count as 0.
Matrix buildMatrix(const ColorMatrix& p) {
  // Coefficients are limited to +-1e6 so a product with a channel stays finite.
  auto value = [&](size_t i) { return std::isfinite(p.values[i]) ? std::clamp(p.values[i], -1.0e6, 1.0e6) : 0.0; };
  if (p.type == QLatin1String("matrix")) {
    if (p.values.size() != 20) return kIdentity;
    Matrix m;
    for (size_t i = 0; i < 20; ++i) m[i] = value(i);
    return m;
  }
  if (p.type == QLatin1String("saturate")) return p.values.size() == 1 ? saturate(std::max(0.0, value(0))) : kIdentity;
  if (p.type == QLatin1String("hueRotate")) return p.values.size() == 1 ? hueRotate(value(0)) : kIdentity;
  if (p.type == QLatin1String("luminanceToAlpha")) return kLuminanceToAlpha;
  return kIdentity;
}

}  // namespace

Layer applyColorMatrix(const Env&, const ColorMatrix& p, ColorSpace cs, const Layer& in) {
  const Matrix m = buildMatrix(p);
  if (m == kIdentity) return in;
  QImage image = inColorSpace(in.image, in.space, cs).copy();
  const float f[20] = {static_cast<float>(m[0]),  static_cast<float>(m[1]),  static_cast<float>(m[2]),
                       static_cast<float>(m[3]),  static_cast<float>(m[4]),  static_cast<float>(m[5]),
                       static_cast<float>(m[6]),  static_cast<float>(m[7]),  static_cast<float>(m[8]),
                       static_cast<float>(m[9]),  static_cast<float>(m[10]), static_cast<float>(m[11]),
                       static_cast<float>(m[12]), static_cast<float>(m[13]), static_cast<float>(m[14]),
                       static_cast<float>(m[15]), static_cast<float>(m[16]), static_cast<float>(m[17]),
                       static_cast<float>(m[18]), static_cast<float>(m[19])};
  const float k = 1.0f / 255.0f;
  for (int y = 0; y < image.height(); ++y) {
    auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      const QRgb p0 = line[x];
      const int a0 = qAlpha(p0);
      const float r = demul(qRed(p0), a0) * k, g = demul(qGreen(p0), a0) * k, b = demul(qBlue(p0), a0) * k;
      const float a = a0 * k;
      float o[4];
      for (int row = 0; row < 4; ++row) {
        const float* c = f + row * 5;
        const float v = c[0] * r + c[1] * g + c[2] * b + c[3] * a + c[4];
        o[row] = v > 0 ? (v < 1 ? v : 1) : 0;  // NaN gives 0
      }
      const int ao = static_cast<int>(o[3] * 255.0f + 0.5f);
      line[x] = qRgba(premul(static_cast<int>(o[0] * 255.0f + 0.5f), ao), premul(static_cast<int>(o[1] * 255.0f + 0.5f), ao),
                      premul(static_cast<int>(o[2] * 255.0f + 0.5f), ao), ao);
    }
  }
  return {image, {}, cs};
}

}  // namespace rnqnx::views::rnsvg::filter::detail
