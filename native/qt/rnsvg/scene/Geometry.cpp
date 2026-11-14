#include "Geometry.h"

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {

bool isShape(Kind k) {
  return k == Kind::Path || k == Kind::Rect || k == Kind::Circle || k == Kind::Ellipse || k == Kind::Line;
}

QPainterPath shapePath(const ElementProps& p, const Units& u) {
  QPainterPath path;
  switch (p.kind) {
    case Kind::Path: path = p.path; break;
    case Kind::Rect: {
      const double x = u.resolve(p.x, Axis::X), y = u.resolve(p.y, Axis::Y);
      const double w = u.resolve(p.width, Axis::X), h = u.resolve(p.height, Axis::Y);
      if (!(w > 0) || !(h > 0)) break;
      double rx = p.rx.set ? u.resolve(p.rx, Axis::X) : (p.ry.set ? u.resolve(p.ry, Axis::Y) : 0);
      double ry = p.ry.set ? u.resolve(p.ry, Axis::Y) : rx;
      rx = std::min(std::max(rx, 0.0), w / 2);
      ry = std::min(std::max(ry, 0.0), h / 2);
      if (rx > 0 && ry > 0) path.addRoundedRect(QRectF(x, y, w, h), rx, ry);
      else path.addRect(QRectF(x, y, w, h));
      break;
    }
    case Kind::Circle: {
      const double r = u.resolve(p.r, Axis::Diag);
      if (!(r > 0)) break;
      path.addEllipse(QPointF(u.resolve(p.cx, Axis::X), u.resolve(p.cy, Axis::Y)), r, r);
      break;
    }
    case Kind::Ellipse: {
      double rx = u.resolve(p.rx, Axis::X), ry = u.resolve(p.ry, Axis::Y);
      if (!p.rx.set) rx = ry;
      if (!p.ry.set) ry = rx;
      if (!(rx > 0) || !(ry > 0)) break;
      path.addEllipse(QPointF(u.resolve(p.cx, Axis::X), u.resolve(p.cy, Axis::Y)), rx, ry);
      break;
    }
    case Kind::Line:
      path.moveTo(u.resolve(p.x1, Axis::X), u.resolve(p.y1, Axis::Y));
      path.lineTo(u.resolve(p.x2, Axis::X), u.resolve(p.y2, Axis::Y));
      break;
    default: break;
  }
  return path;
}

std::vector<PathVertex> shapeVertices(const ElementProps& p, const Units& u) {
  if (p.kind == Kind::Path) return p.vertices;
  std::vector<PathVertex> out;
  if (p.kind != Kind::Line) return out;
  const QPointF a(u.resolve(p.x1, Axis::X), u.resolve(p.y1, Axis::Y));
  const QPointF b(u.resolve(p.x2, Axis::X), u.resolve(p.y2, Axis::Y));
  QPointF d = b - a;
  const double len = std::hypot(d.x(), d.y());
  d = len > 0 ? d / len : QPointF();
  PathVertex start;
  start.point = a;
  start.outDirection = d;
  start.subpathStart = true;
  PathVertex end;
  end.point = b;
  end.inDirection = d;
  end.subpathEnd = true;
  out.push_back(start);
  out.push_back(end);
  return out;
}

QTransform viewBoxTransform(const ViewBox& box, const QSizeF& size) {
  if (!box.set || !(box.width > 0) || !(box.height > 0) || !(size.width() > 0) || !(size.height() > 0)) {
    return QTransform();
  }
  const double sx = size.width() / box.width, sy = size.height() / box.height;
  if (box.meetOrSlice == 2 || box.align == QStringLiteral("none")) {
    return QTransform(sx, 0, 0, sy, -box.minX * sx, -box.minY * sy);
  }
  const double s = box.meetOrSlice == 1 ? std::max(sx, sy) : std::min(sx, sy);
  const double fx = box.align.startsWith(QStringLiteral("xMin")) ? 0 : box.align.startsWith(QStringLiteral("xMax")) ? 1 : 0.5;
  const double fy = box.align.endsWith(QStringLiteral("YMin")) ? 0 : box.align.endsWith(QStringLiteral("YMax")) ? 1 : 0.5;
  return QTransform(s, 0, 0, s, (size.width() - box.width * s) * fx - box.minX * s,
                    (size.height() - box.height * s) * fy - box.minY * s);
}

}  // namespace rnqnx::views::rnsvg
