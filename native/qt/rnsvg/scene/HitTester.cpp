#include "HitTester.h"

#include "Scene.h"
#include "ShapePainter.h"

#include <QPainterPathStroker>

#include <algorithm>

namespace rnqnx::views::rnsvg {

PointerMode PointerMode::parse(const QString& v) {
  using R = Region;
  if (v == QStringLiteral("none")) return {R::None, false};
  if (v == QStringLiteral("box-only")) return {R::Box, false};
  if (v == QStringLiteral("box-none")) return {R::None, true};
  if (v == QStringLiteral("visibleFill") || v == QStringLiteral("fill")) return {R::Fill, true};
  if (v == QStringLiteral("visibleStroke") || v == QStringLiteral("stroke")) return {R::Stroke, true};
  if (v == QStringLiteral("visible") || v == QStringLiteral("all")) return {R::All, true};
  return {R::Painted, true};  // auto, visiblePainted, painted and the unset default
}

void HitTester::testPath(const RnsvgElementItem* e, const QPainterPath& pathIn, const Frame& f,
                         const PointerMode& mode) {
  const auto& point = scene_.hitPoint();
  if (!point || mode.region == PointerMode::Region::None) return;
  bool invertible = false;
  const QTransform inverse = f.ctm.inverted(&invertible);
  if (!invertible) return;
  const QPointF pt = inverse.map(*point);
  const Style& st = f.style;
  const Units units(f);
  QPainterPath path = pathIn;
  path.setFillRule(st.fillEvenOdd ? Qt::OddEvenFill : Qt::WindingFill);

  using R = PointerMode::Region;
  const bool fillArea = mode.region == R::Fill || mode.region == R::All ||
                        (mode.region == R::Painted && st.fill.type != Brush::Type::None);
  const double w = ShapePainter::strokeWidth(st, units);
  const bool strokeArea = w > 0 && (mode.region == R::Stroke || mode.region == R::All ||
                                    (mode.region == R::Painted && st.stroke.type != Brush::Type::None));
  bool inside = false;
  if (mode.region == R::Box) {
    inside = path.boundingRect().contains(pt);
  } else {
    if (fillArea && path.contains(pt)) inside = true;
    if (!inside && strokeArea) {
      const double reach = w * std::max(st.miter, 1.0);
      if (path.boundingRect().adjusted(-reach, -reach, reach, reach).contains(pt)) {
        QPainterPathStroker stroker;
        stroker.setWidth(w);
        stroker.setCapStyle(ShapePainter::capStyle(st.cap));
        stroker.setJoinStyle(ShapePainter::joinStyle(st.join));
        stroker.setMiterLimit(st.miter);
        inside = stroker.createStroke(path).contains(pt);
      }
    }
  }
  if (inside) scene_.noteHit(e);
}

}  // namespace rnqnx::views::rnsvg
