#include "ShapePainter.h"

#include "Scene.h"

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {
namespace {

// Dashes this small explode into more geometry than the pixels can show.
constexpr double kMinDashSum = 0.05;

}  // namespace

Qt::PenCapStyle ShapePainter::capStyle(int cap) {
  return cap == 1 ? Qt::RoundCap : cap == 2 ? Qt::SquareCap : Qt::FlatCap;
}

Qt::PenJoinStyle ShapePainter::joinStyle(int join) {
  return join == 1 ? Qt::RoundJoin : join == 2 ? Qt::BevelJoin : Qt::MiterJoin;
}

double ShapePainter::strokeWidth(const Style& st, const Units& u) {
  const double w = u.resolve(st.strokeWidth, Axis::Diag, 1);
  return std::isfinite(w) && w > 0 ? w : 0;
}

QPen ShapePainter::pen(const QBrush& brush, const Style& st, const Units& u, double itemScale) {
  double w = strokeWidth(st, u);
  // vectorEffect="non-scaling-stroke": the width and dashes are in item
  // pixels, so divide the current scale back out of them.
  const double k = st.nonScaling && itemScale > 0 ? 1.0 / itemScale : 1.0;
  w *= k;
  QPen pen(brush, w);
  pen.setCapStyle(capStyle(st.cap));
  pen.setJoinStyle(joinStyle(st.join));
  pen.setMiterLimit(st.miter);
  if (!st.dash.empty() && w > 0) {
    QList<qreal> pattern;
    double sum = 0;
    for (const auto& d : st.dash) {
      const double v = std::max(u.resolve(d, Axis::Diag) * k, 0.0);
      pattern.append(v / w);
      sum += v;
    }
    if (sum > 0 && sum >= kMinDashSum * std::max(w, 1.0)) {
      if (pattern.size() % 2) pattern += pattern;
      pen.setDashPattern(pattern);
      pen.setDashOffset(st.dashOffset * k / w);
    }
  }
  return pen;
}

void ShapePainter::draw(const QPainterPath& pathIn, const Frame& f, const QRectF& bbox) {
  const Style& st = f.style;
  QPainter* painter = scene_.painter();
  QPainterPath path = pathIn;
  path.setFillRule(st.fillEvenOdd ? Qt::OddEvenFill : Qt::WindingFill);
  PaintBrush fill, stroke;
  if (scene_.paintServers().make(st.fill, bbox, f, st.fillOpacity, fill)) {
    const double saved = painter->opacity();
    painter->setOpacity(saved * fill.opacity);
    painter->fillPath(path, fill.brush);
    painter->setOpacity(saved);
  }
  const Units u(f);
  if (strokeWidth(st, u) > 0 && scene_.paintServers().make(st.stroke, bbox, f, st.strokeOpacity, stroke)) {
    // The scale a non-scaling stroke undoes: the geometric mean scale of the
    // transform from this element's user space to item space. The texture's
    // own device pixel ratio stays in, so the stroke is as wide in item
    // pixels as it asked for.
    const double scale = std::sqrt(std::abs(f.ctm.determinant()));
    const double saved = painter->opacity();
    painter->setOpacity(saved * stroke.opacity);
    painter->strokePath(path, pen(stroke.brush, st, u, scale));
    painter->setOpacity(saved);
  }
}

}  // namespace rnqnx::views::rnsvg
