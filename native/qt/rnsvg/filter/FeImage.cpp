#include "FilterPrimitives.h"

#include <QPainter>
#include <algorithm>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

// preserveAspectRatio: where an image of `imageSize` lands inside `box`.
QRectF fitImage(const QSizeF& imageSize, const QRectF& box, const Image& p) {
  if (imageSize.width() <= 0 || imageSize.height() <= 0) return {};
  const double sx = box.width() / imageSize.width(), sy = box.height() / imageSize.height();
  if (p.meetOrSlice == 2 || p.align == QLatin1String("none")) return box;
  const double s = p.meetOrSlice == 1 ? std::max(sx, sy) : std::min(sx, sy);
  const double w = imageSize.width() * s, h = imageSize.height() * s;
  const QString& a = p.align;
  const double fx = a.contains(QLatin1String("xMin")) ? 0.0 : (a.contains(QLatin1String("xMax")) ? 1.0 : 0.5);
  const double fy = a.contains(QLatin1String("YMin")) ? 0.0 : (a.contains(QLatin1String("YMax")) ? 1.0 : 0.5);
  return QRectF(box.x() + (box.width() - w) * fx, box.y() + (box.height() - h) * fy, w, h);
}

}  // namespace

// Two sources, both drawn into the subregion and clipped to it, both tagged
// sRGB because that is what an image holds:
//  - href "#id": Context::renderElement draws the element in the filtered
//    element's user space translated to the subregion's x and y (the filter
//    region's corner when the subregion is not set), which is what resvg does
//    and what the Filter Effects text means by "like the use element".
//  - anything else: Context::loadImage gives the pixels, which are fitted into
//    the user-space subregion by preserveAspectRatio and drawn through the
//    transform, so a rotated or scaled CTM carries over.
// A missing callback, an unknown id or an undecodable image gives a
// transparent result.
Layer applyImage(const Env& env, const Image& p, const QRect& sub, const QRectF& userSub) {
  Layer out{makeImage(env.region.size()), {}, ColorSpace::SRGB};
  const Context& c = *env.context;
  QPainter painter(&out.image);
  painter.setClipRect(sub.intersected(out.image.rect()));
  if (p.href.startsWith(QLatin1Char('#'))) {
    if (!c.renderElement) return out;
    const QTransform t = QTransform::fromTranslate(userSub.x(), userSub.y()) * env.toDevice;
    const QImage element = c.renderElement(p.href.mid(1), t, env.region);
    if (element.isNull()) return out;
    painter.drawImage(QPoint(0, 0), element.convertToFormat(QImage::Format_ARGB32_Premultiplied));
    return out;
  }
  if (!c.loadImage) return out;
  const QImage image = c.loadImage(p.href);
  if (image.isNull()) return out;
  const QRectF dest = fitImage(image.size(), userSub, p);
  if (dest.isEmpty()) return out;
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.setTransform(env.toDevice * QTransform::fromTranslate(-env.region.x(), -env.region.y()));
  painter.drawImage(dest, image, QRectF(QPointF(0, 0), QSizeF(image.size())));
  return out;
}

}  // namespace rnqnx::views::rnsvg::filter::detail
