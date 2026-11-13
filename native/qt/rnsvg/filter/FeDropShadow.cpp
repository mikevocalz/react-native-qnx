#include "FilterColorSpace.h"
#include "FilterPrimitives.h"
#include "GaussianBlur.h"

#include <algorithm>
#include <cstring>

namespace rnqnx::views::rnsvg::filter::detail {

// feDropShadow is feGaussianBlur of the input alpha, feOffset, feFlood and
// feMerge in one step. A negative or non-finite deviation counts as 0 (the
// shadow is not blurred). The shadow color is an sRGB value converted to the
// working space.
Layer applyDropShadow(const Env& env, const DropShadow& p, ColorSpace cs, const Layer& in) {
  const QImage src = inColorSpace(in.image, in.space, cs);
  const QSize size = src.size();
  const QColor color = brushColor(p.color, env, QColor(Qt::black));
  const QRgb flood = premultipliedIn(color, p.opacity, cs);
  const int fa = qAlpha(flood);

  QImage shadow = makeImage(size);
  for (int y = 0; y < size.height(); ++y) {
    const auto* s = reinterpret_cast<const QRgb*>(src.constScanLine(y));
    auto* d = reinterpret_cast<QRgb*>(shadow.scanLine(y));
    for (int x = 0; x < size.width(); ++x) {
      const int a = qAlpha(s[x]);
      if (a == 0) continue;
      d[x] = qRgba(div255(qRed(flood) * a), div255(qGreen(flood) * a), div255(qBlue(flood) * a), div255(fa * a));
    }
  }
  auto sigma = [](double v) { return std::isfinite(v) && v > 0 ? v : 0.0; };
  const QPointF dev = env.extentToDevice(env.userNumber(sigma(p.stdDeviationX), Axis::X),
                                         env.userNumber(sigma(p.stdDeviationY), Axis::Y));
  gaussianBlur(shadow, dev.x(), dev.y(), EdgeMode::None);

  const QPointF v = env.vectorToDevice(env.userLength(p.dx, Axis::X), env.userLength(p.dy, Axis::Y));
  QImage out = makeImage(size);
  if (std::isfinite(v.x()) && std::isfinite(v.y()) && std::abs(v.x()) < size.width() && std::abs(v.y()) < size.height()) {
    const int dx = static_cast<int>(std::lround(v.x())), dy = static_cast<int>(std::lround(v.y()));
    const int x0 = std::max(0, dx), x1 = std::min(size.width(), size.width() + dx);
    for (int y = std::max(0, dy); x1 > x0 && y < std::min(size.height(), size.height() + dy); ++y) {
      std::memcpy(out.scanLine(y) + x0 * 4, shadow.constScanLine(y - dy) + (x0 - dx) * 4, static_cast<size_t>(x1 - x0) * 4);
    }
  }
  for (int y = 0; y < size.height(); ++y) {
    const auto* s = reinterpret_cast<const QRgb*>(src.constScanLine(y));
    auto* d = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < size.width(); ++x) d[x] = over(s[x], d[x]);
  }
  return {out, {}, cs};
}

}  // namespace rnqnx::views::rnsvg::filter::detail
