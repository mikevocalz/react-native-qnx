#include "PathGlyphs.h"

#include <QPolygonF>
#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg::text::detail {

namespace {

constexpr double kRadToDeg = 180.0 / M_PI;
// Outline curves are flattened at this multiple of the glyph's pixel size,
// then long straight edges are split so they can bend with the path.
constexpr double kFlattenScale = 4;

double angleOf(QPointF v) { return std::atan2(v.y(), v.x()) * kRadToDeg; }

// SVG 1.1 section 10.14: a glyph is shown only if its midpoint is on the path.
// A closed path is traversed once, from the start offset, in the direction the
// text-anchor extends the text.
bool midpointOnPath(const PathRun& run, double mid, Anchor anchor) {
  if (!std::isfinite(mid)) return false;
  const double length = run.measure.length();
  if (!run.measure.closed()) return mid >= 0 && mid <= length;
  double first = run.startOffset;
  if (anchor == Anchor::Middle) first -= length / 2;
  else if (anchor == Anchor::End) first -= length;
  return mid >= first && mid <= first + length;
}

// method=stretch: every outline point moves onto the path, along the normal
// at its own distance, so a glyph bends with a curved path.
QPainterPath warpOutline(const PathRun& run, const Unit& u, double start, double rotationDeg) {
  const double sx = u.size * u.scaleX, sy = u.size;
  const double width = u.advance * u.scaleX;
  const double maxEdge = std::max(u.size / 8, 1e-3);
  QTransform pre;
  pre.translate(width / 2, 0);
  pre.rotate(rotationDeg);
  pre.translate(-width / 2, 0);

  QPainterPath out;
  out.setFillRule(Qt::WindingFill);
  const QList<QPolygonF> polygons = u.outline.toSubpathPolygons(QTransform::fromScale(sx * kFlattenScale, sy * kFlattenScale));
  for (const QPolygonF& polygon : polygons) {
    if (polygon.size() < 2) continue;
    QPolygonF mapped;
    for (qsizetype i = 0; i < polygon.size(); ++i) {
      const QPointF a = polygon[i] / kFlattenScale;
      const QPointF b = i + 1 < polygon.size() ? polygon[i + 1] / kFlattenScale : a;
      const int pieces = std::clamp(int(std::ceil(std::hypot(b.x() - a.x(), b.y() - a.y()) / maxEdge)), 1, 64);
      for (int k = 0; k < pieces; ++k) {
        const QPointF local = pre.map(a + (b - a) * (double(k) / pieces));
        const PathMeasure::Sample at = run.measure.at(start + local.x());
        const QPointF normal(-at.tangent.y(), at.tangent.x());
        mapped << at.point + normal * (local.y() + u.perp);
      }
    }
    out.addPolygon(mapped);
    out.closeSubpath();
  }
  return out;
}

}  // namespace

PathRun::PathRun(const TextPathSpec& s, bool& truncated)
    : spec(s), measure(s.path, s.side == PathSide::Right, truncated) {
  const double offset = s.startOffset.value;
  startOffset = std::isfinite(offset) ? (s.startOffset.percent ? offset / 100 * measure.length() : offset) : 0;
}

PathGlyph placeOnPath(const PathRun& run, const Unit& u, Anchor chunkAnchor) {
  PathGlyph g;
  if (!run.measure.valid()) return g;
  const double width = u.advance * u.scaleX;
  const double start = run.startOffset + u.pos;
  const double mid = start + width / 2;
  if (!midpointOnPath(run, mid, chunkAnchor)) return g;

  const PathMeasure::Sample at = run.measure.at(mid);
  double angle = angleOf(at.tangent);
  if (run.spec.midLine == PathMidLine::Smooth && width > 0) {
    // Smooth: aim the glyph along the chord of the path under it, which turns
    // gradually through a corner instead of snapping at the midpoint.
    const QPointF chord = run.measure.at(mid + width / 2).point - run.measure.at(mid - width / 2).point;
    if (std::hypot(chord.x(), chord.y()) > 1e-9) angle = angleOf(chord);
  }
  g.rotation = angle + u.rotate;
  g.origin = run.measure.at(start).point;
  g.frame.translate(at.point.x(), at.point.y());
  g.frame.rotate(g.rotation);
  g.frame.translate(-width / 2, u.perp);
  g.visible = true;
  if (u.space || u.outline.isEmpty()) return g;
  if (run.spec.method == PathMethod::Stretch) {
    g.outline = warpOutline(run, u, start, u.rotate);
  } else {
    QTransform scale;
    scale.scale(u.size * u.scaleX, u.size);
    g.outline = g.frame.map(scale.map(u.outline));
  }
  return g;
}

}  // namespace rnqnx::views::rnsvg::text::detail
