#include "PaintServers.h"

#include "Scene.h"

#include <QLinearGradient>
#include <QRadialGradient>

#include <algorithm>

namespace rnqnx::views::rnsvg {

bool PaintServers::make(const Brush& b, const QRectF& bbox, const Frame& f, double opacity, PaintBrush& out) {
  switch (b.type) {
    case Brush::Type::None: return false;
    case Brush::Type::Color:
      out = {QBrush(withAlpha(b.color, opacity)), 1};
      return true;
    case Brush::Type::CurrentColor:
      out = {QBrush(withAlpha(f.style.color, opacity)), 1};
      return true;
    case Brush::Type::ContextFill:
    case Brush::Type::ContextStroke: {
      const auto* c = scene_.context();
      if (!c) return false;  // outside a marker or <use>: context paint is `none`
      const Brush& source = b.type == Brush::Type::ContextFill ? c->fill : c->stroke;
      if (source.type == Brush::Type::ContextFill || source.type == Brush::Type::ContextStroke) return false;
      return make(source, c->bbox, c->frame, opacity, out);
    }
    case Brush::Type::Ref: {
      const auto* server = scene_.byId(b.ref);
      if (!server) return false;
      const auto& p = server->props();
      if (p.kind == Kind::LinearGradient || p.kind == Kind::RadialGradient) return gradient(p, bbox, f, opacity, out);
      if (p.kind == Kind::Pattern) return pattern(*server, bbox, f, opacity, out);
      return false;
    }
  }
  return false;
}

bool PaintServers::gradient(const ElementProps& g, const QRectF& bbox, const Frame& f, double opacity,
                            PaintBrush& out) {
  if (g.stops.empty()) return false;
  const Units u(f);
  const bool user = g.gradientUserSpace;
  if (!user && (!(bbox.width() > 0) || !(bbox.height() > 0))) return false;
  QGradientStops stops;
  for (const auto& s : g.stops) stops.append({std::clamp(s.offset, 0.0, 1.0), withAlpha(s.color, opacity)});
  QTransform t;
  if (!user) t = QTransform(bbox.width(), 0, 0, bbox.height(), bbox.x(), bbox.y());
  if (g.gradientTransform) t = *g.gradientTransform * t;
  const QGradient::Spread spread = g.spreadMethod == 1   ? QGradient::ReflectSpread
                                   : g.spreadMethod == 2 ? QGradient::RepeatSpread
                                                         : QGradient::PadSpread;
  const QSizeF vp = f.viewport;
  if (g.kind == Kind::LinearGradient) {
    QLinearGradient grad(u.resolveUnits(g.x1, user, Axis::X, 0), u.resolveUnits(g.y1, user, Axis::Y, 0),
                         u.resolveUnits(g.x2, user, Axis::X, user ? vp.width() : 1),
                         u.resolveUnits(g.y2, user, Axis::Y, 0));
    grad.setStops(stops);
    grad.setSpread(spread);
    QBrush brush(grad);
    brush.setTransform(t);
    out = {brush, 1};
    return true;
  }
  const double cx = u.resolveUnits(g.cx, user, Axis::X, user ? vp.width() / 2 : 0.5);
  const double cy = u.resolveUnits(g.cy, user, Axis::Y, user ? vp.height() / 2 : 0.5);
  const double rx = u.resolveUnits(g.rx, user, Axis::X, user ? vp.width() / 2 : 0.5);
  const double ry = u.resolveUnits(g.ry, user, Axis::Y, user ? vp.height() / 2 : 0.5);
  if (!(rx > 0) || !(ry > 0)) return false;
  const double fx = u.resolveUnits(g.fx, user, Axis::X, cx);
  const double fy = u.resolveUnits(g.fy, user, Axis::Y, cy);
  // An elliptical gradient is a circular one scaled about its center.
  QRadialGradient grad(QPointF(cx, cy), rx, QPointF(fx, cy + (fy - cy) * rx / ry));
  grad.setStops(stops);
  grad.setSpread(spread);
  QBrush brush(grad);
  brush.setTransform(QTransform::fromTranslate(0, cy).scale(1, ry / rx).translate(0, -cy) * t);
  out = {brush, 1};
  return true;
}

}  // namespace rnqnx::views::rnsvg
