// <pattern> as a paint server: one tile rendered to an image at the device
// scale of the filled element, repeated by Qt as a texture brush.

#include "Geometry.h"
#include "PaintServers.h"
#include "Scene.h"

#include <QImage>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {
namespace {

constexpr int kMaxPatternDepth = 3;   // a pattern whose content is filled by a pattern ...
constexpr int kMaxTilePixels = 2048;  // per side

}  // namespace

bool PaintServers::pattern(const RnsvgElementItem& server, const QRectF& bbox, const Frame& f, double opacity,
                           PaintBrush& out) {
  if (patternDepth_ >= kMaxPatternDepth) return false;
  const ElementProps& p = server.props();
  const Units u(f);
  const bool user = p.patternUserSpace;
  QRectF tile;
  if (user) {
    tile = QRectF(u.resolve(p.x, Axis::X), u.resolve(p.y, Axis::Y), u.resolve(p.width, Axis::X),
                  u.resolve(p.height, Axis::Y));
  } else {
    if (!(bbox.width() > 0) || !(bbox.height() > 0)) return false;
    tile = QRectF(bbox.x() + u.resolveUnits(p.x, false, Axis::X, 0) * bbox.width(),
                  bbox.y() + u.resolveUnits(p.y, false, Axis::Y, 0) * bbox.height(),
                  u.resolveUnits(p.width, false, Axis::X, 1) * bbox.width(),
                  u.resolveUnits(p.height, false, Axis::Y, 1) * bbox.height());
  }
  if (!std::isfinite(tile.x()) || !std::isfinite(tile.y()) || !(tile.width() > 0) || !(tile.height() > 0)) return false;

  const QTransform patternTransform = p.patternTransform ? *p.patternTransform : p.matrix.value_or(QTransform());
  const QTransform device = patternTransform * f.ctm * scene_.target().base;
  const double sx = std::hypot(device.m11(), device.m12());
  const double sy = std::hypot(device.m21(), device.m22());
  if (!(sx > 0) || !(sy > 0) || !std::isfinite(sx) || !std::isfinite(sy)) return false;
  // Clamp as doubles: a huge tile must not overflow the int conversion.
  const int w = int(std::clamp(std::ceil(tile.width() * sx), 1.0, double(kMaxTilePixels)));
  const int h = int(std::clamp(std::ceil(tile.height() * sy), 1.0, double(kMaxTilePixels)));
  const double kx = w / tile.width(), ky = h / tile.height();

  QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) return false;
  image.fill(Qt::transparent);

  Frame content;
  content.style = Style();
  content.style.color = f.style.color;
  applyProps(content.style, p);
  content.viewport = f.viewport;
  if (p.viewBox.set) {
    content.ctm = viewBoxTransform(p.viewBox, tile.size());
    content.viewport = QSizeF(p.viewBox.width, p.viewBox.height);
  } else if (!p.patternContentUserSpace) {
    // patternContentUnits=objectBoundingBox: content coordinates are fractions
    // of the filled element's box.
    content.ctm = QTransform::fromScale(bbox.width(), bbox.height());
  }

  {
    QPainter painter(&image);
    Scene::configure(painter);
    Target target;
    target.painter = &painter;
    target.base = QTransform::fromScale(kx, ky);
    target.clip = QRectF(0, 0, w, h);
    painter.setTransform(target.base);
    painter.setClipRect(QRectF(0, 0, tile.width(), tile.height()));
    ++patternDepth_;
    scene_.withoutHit([&] { scene_.withTarget(target, [&] { scene_.renderChildren(&server, content, 0); }); });
    --patternDepth_;
  }

  QBrush brush(image);
  brush.setTransform(QTransform::fromScale(1.0 / kx, 1.0 / ky) * QTransform::fromTranslate(tile.x(), tile.y()) *
                     patternTransform);
  out = {brush, opacity};
  return true;
}

}  // namespace rnqnx::views::rnsvg
