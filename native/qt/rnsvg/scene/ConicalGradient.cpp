#include "ConicalGradient.h"

#include <QImage>

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {
namespace {

constexpr int kMaxTextureSide = 2048;

QColor colorAt(const QGradientStops& stops, double t) {
  if (t <= stops.front().first) return stops.front().second;
  if (t >= stops.back().first) return stops.back().second;
  for (int i = 1; i < stops.size(); ++i) {
    if (t > stops[i].first) continue;
    const double span = stops[i].first - stops[i - 1].first;
    const double k = span > 0 ? (t - stops[i - 1].first) / span : 1.0;
    const QColor a = stops[i - 1].second, b = stops[i].second;
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * k, a.greenF() + (b.greenF() - a.greenF()) * k,
                            a.blueF() + (b.blueF() - a.blueF()) * k, a.alphaF() + (b.alphaF() - a.alphaF()) * k);
  }
  return stops.back().second;
}

}  // namespace

QBrush makeConicalGradientBrush(const QGradientStops& stops, QPointF center, double radiusX, double radiusY,
                                QPointF focal, const QTransform& gradientToShape, const QRectF& area, double scale) {
  bool invertible = false;
  const QTransform shapeToGradient = gradientToShape.inverted(&invertible);
  if (!invertible || stops.isEmpty() || !(radiusX > 0) || !(radiusY > 0) || !area.isValid() || !(scale > 0)) {
    return QBrush(Qt::transparent);
  }
  const int w = int(std::clamp(std::ceil(area.width() * scale), 1.0, double(kMaxTextureSide)));
  const int h = int(std::clamp(std::ceil(area.height() * scale), 1.0, double(kMaxTextureSide)));
  const double sx = w / area.width(), sy = h / area.height();
  // Normalized space: the end circle is the unit circle at the origin, the
  // start "circle" is the focal point F with radius 0. The gradient value at P
  // is the largest t >= 0 with |P - F(1 - t)| = t, the circle at parameter t.
  const QPointF focalN((focal.x() - center.x()) / radiusX, (focal.y() - center.y()) / radiusY);
  const double a = focalN.x() * focalN.x() + focalN.y() * focalN.y() - 1.0;  // > 0 outside the circle
  QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  if (!(a > 0)) return QBrush(image);
  for (int j = 0; j < h; ++j) {
    auto* line = reinterpret_cast<QRgb*>(image.scanLine(j));
    for (int i = 0; i < w; ++i) {
      const QPointF shape(area.x() + (i + 0.5) / sx, area.y() + (j + 0.5) / sy);
      const QPointF g = shapeToGradient.map(shape);
      const QPointF u(((g.x() - center.x()) / radiusX) - focalN.x(), ((g.y() - center.y()) / radiusY) - focalN.y());
      const double b = 2.0 * (u.x() * focalN.x() + u.y() * focalN.y());
      const double c = u.x() * u.x() + u.y() * u.y();
      const double disc = b * b - 4.0 * a * c;
      if (disc < 0) continue;  // outside the cone
      const double t = (-b + std::sqrt(disc)) / (2.0 * a);
      if (t < 0) continue;
      line[i] = qPremultiply(colorAt(stops, std::min(t, 1.0)).rgba());
    }
  }
  QBrush brush(image);
  brush.setTransform(QTransform::fromScale(1.0 / sx, 1.0 / sy) * QTransform::fromTranslate(area.x(), area.y()));
  return brush;
}

}  // namespace rnqnx::views::rnsvg
