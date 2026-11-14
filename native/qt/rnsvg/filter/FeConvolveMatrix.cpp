#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

// pixels * taps above this is treated as an invalid primitive; it keeps one
// feConvolveMatrix to roughly a second on a Cortex-A72.
constexpr qint64 kMaxWork = qint64{1} << 28;

int edgePixel(int i, int n, EdgeMode mode) {
  if (i >= 0 && i < n) return i;
  switch (mode) {
    case EdgeMode::None: return -1;
    case EdgeMode::Duplicate: return i < 0 ? 0 : n - 1;
    case EdgeMode::Wrap: {
      const int r = i % n;
      return r < 0 ? r + n : r;
    }
  }
  return -1;
}

}  // namespace

// The kernel is applied as the specification defines it (SOURCE index runs
// forward, kernelMatrix runs backward). With preserveAlpha the colors are
// convolved demultiplied and re-premultiplied with the source alpha; without
// it all four premultiplied channels are convolved and bias is scaled by the
// result alpha. An invalid primitive (order outside 1..32, a kernelMatrix of
// the wrong length, a target outside the kernel, a non-finite divisor or too
// much work) gives a transparent result.
Layer applyConvolveMatrix(const Env& env, const ConvolveMatrix& p, ColorSpace cs, const QRect& sub, const Layer& in) {
  Layer out{makeImage(env.region.size()), {}, cs};
  const int ox = p.orderX, oy = p.orderY;
  if (ox < 1 || oy < 1 || ox > kMaxConvolveOrder || oy > kMaxConvolveOrder) return out;
  if (p.kernelMatrix.size() != static_cast<size_t>(ox) * static_cast<size_t>(oy)) return out;
  const int tx = p.targetX < 0 ? ox / 2 : p.targetX, ty = p.targetY < 0 ? oy / 2 : p.targetY;
  if (tx >= ox || ty >= oy) return out;
  double divisor = p.divisor;
  if (!std::isfinite(divisor)) return out;
  if (divisor == 0) {
    double sum = 0;
    for (double k : p.kernelMatrix) sum += std::isfinite(k) ? k : 0.0;
    divisor = sum == 0 ? 1.0 : sum;
  }
  const float bias = std::isfinite(p.bias) ? static_cast<float>(std::clamp(p.bias, -1.0e6, 1.0e6)) : 0.0f;
  const QRect r = sub.intersected(out.image.rect());
  if (r.isEmpty()) return out;
  if (static_cast<qint64>(r.width()) * r.height() * ox * oy > kMaxWork) return out;

  const QImage src = inColorSpace(in.image, in.space, cs);
  const int w = src.width(), h = src.height();
  // Source samples as bytes: premultiplied, or demultiplied color + alpha.
  std::vector<quint8> data(static_cast<size_t>(w) * h * 4);
  for (int y = 0; y < h; ++y) {
    const auto* s = reinterpret_cast<const QRgb*>(src.constScanLine(y));
    quint8* d = data.data() + static_cast<size_t>(y) * w * 4;
    for (int x = 0; x < w; ++x) {
      const QRgb px = s[x];
      const int a = qAlpha(px);
      d[4 * x + 3] = static_cast<quint8>(a);
      if (p.preserveAlpha) {
        d[4 * x] = static_cast<quint8>(demul(qRed(px), a));
        d[4 * x + 1] = static_cast<quint8>(demul(qGreen(px), a));
        d[4 * x + 2] = static_cast<quint8>(demul(qBlue(px), a));
      } else {
        d[4 * x] = static_cast<quint8>(qRed(px));
        d[4 * x + 1] = static_cast<quint8>(qGreen(px));
        d[4 * x + 2] = static_cast<quint8>(qBlue(px));
      }
    }
  }
  std::vector<float> kernel(p.kernelMatrix.size());
  for (int i = 0; i < oy; ++i) {
    for (int j = 0; j < ox; ++j) {
      const double k = p.kernelMatrix[static_cast<size_t>(oy - i - 1) * ox + static_cast<size_t>(ox - j - 1)];
      const double weight = k / divisor;  // a tiny divisor can overflow a float
      kernel[static_cast<size_t>(i) * ox + j] =
          std::isfinite(k) && std::isfinite(weight) ? static_cast<float>(std::clamp(weight, -1.0e6, 1.0e6)) : 0.0f;
    }
  }
  const float inv = 1.0f / 255.0f;
  for (int y = r.top(); y <= r.bottom(); ++y) {
    auto* line = reinterpret_cast<QRgb*>(out.image.scanLine(y));
    for (int x = r.left(); x <= r.right(); ++x) {
      float acc[4] = {0, 0, 0, 0};
      for (int i = 0; i < oy; ++i) {
        const int sy = edgePixel(y - ty + i, h, p.edgeMode);
        if (sy < 0) continue;
        for (int j = 0; j < ox; ++j) {
          const int sx = edgePixel(x - tx + j, w, p.edgeMode);
          if (sx < 0) continue;
          const quint8* s = data.data() + (static_cast<size_t>(sy) * w + sx) * 4;
          const float k = kernel[static_cast<size_t>(i) * ox + j];
          acc[0] += k * s[0];
          acc[1] += k * s[1];
          acc[2] += k * s[2];
          acc[3] += k * s[3];
        }
      }
      auto clamp1 = [](float v) { return v > 0 ? (v < 1 ? v : 1.0f) : 0.0f; };  // NaN gives 0
      if (p.preserveAlpha) {
        const int a = data[(static_cast<size_t>(y) * w + x) * 4 + 3];
        auto c = [&](float v) { return premul(static_cast<int>(clamp1(v * inv + bias) * 255.0f + 0.5f), a); };
        line[x] = qRgba(c(acc[0]), c(acc[1]), c(acc[2]), a);
      } else {
        const float a = clamp1(acc[3] * inv + bias);
        auto c = [&](float v) { return std::min(a, clamp1(v * inv + bias * a)); };
        line[x] = qRgba(static_cast<int>(c(acc[0]) * 255.0f + 0.5f), static_cast<int>(c(acc[1]) * 255.0f + 0.5f),
                        static_cast<int>(c(acc[2]) * 255.0f + 0.5f), static_cast<int>(a * 255.0f + 0.5f));
      }
    }
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::filter::detail
