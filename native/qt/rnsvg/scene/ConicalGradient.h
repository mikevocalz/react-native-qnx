#pragma once

#include <QBrush>
#include <QGradientStops>
#include <QRectF>
#include <QTransform>

namespace rnqnx::views::rnsvg {

// A radial gradient whose focal point lies outside its circle. SVG 2 paints a
// cone from the focal point to the circle and nothing outside it; Qt's
// QRadialGradient moves such a focal point onto the circle instead, so the
// cone is rasterised into a texture.
//
// `center`, `radiusX` and `radiusY` and `focal` are in gradient space;
// `gradientToShape` maps gradient space to the shape's user space; `area` is
// the part of the shape's user space the texture must cover; `scale` is
// texels per user unit.
QBrush makeConicalGradientBrush(const QGradientStops& stops, QPointF center, double radiusX, double radiusY,
                                QPointF focal, const QTransform& gradientToShape, const QRectF& area, double scale);

}  // namespace rnqnx::views::rnsvg
