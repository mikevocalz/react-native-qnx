#include "Bounds.h"

#include "Geometry.h"
#include "Scene.h"
#include "ShapePainter.h"

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {
namespace {

QRectF unionRect(const QRectF& a, const QRectF& b) {
  if (a.isNull()) return b;
  if (b.isNull()) return a;
  return a.united(b);
}

// How far a stroke reaches past the outline: half the width, times the miter
// limit for miter joins, with square caps reaching sqrt(2) times half the width.
double strokeReach(const Style& st, double width) {
  double factor = 1.0;
  if (st.join == 0) factor = std::max(factor, std::max(st.miter, 1.0));
  if (st.cap == 2) factor = std::max(factor, std::sqrt(2.0));
  return width * 0.5 * factor;
}

}  // namespace

QRectF Bounds::object(const RnsvgElementItem* e, const Frame& inherited, int depth) {
  return compute(e, inherited, depth, false);
}

QRectF Bounds::paint(const RnsvgElementItem* e, const Frame& inherited, int depth) {
  return compute(e, inherited, depth, true);
}

QRectF Bounds::compute(const RnsvgElementItem* e, const Frame& inherited, int depth, bool painted) {
  if (depth > kMaxDepth) return {};
  const auto& p = e->props();
  if (p.displayNone) return {};
  Frame f = inherited;
  applyProps(f.style, p);
  const Units units(f);
  QRectF box;
  if (isShape(p.kind)) {
    const QPainterPath path = shapePath(p, units);
    box = path.boundingRect();
    if (painted && !path.isEmpty()) {
      const double w = ShapePainter::strokeWidth(f.style, units);
      if (f.style.stroke.type != Brush::Type::None && w > 0) {
        const double r = strokeReach(f.style, w);
        box.adjust(-r, -r, r, r);
      }
      box = unionRect(box, scene_.markers().bounds(e, f, depth));
    }
  } else if (p.kind == Kind::Group || p.kind == Kind::Symbol) {
    for (auto* c : e->elementChildren()) {
      QRectF cb = compute(c, f, depth + 1, painted);
      if (!cb.isNull() && c->props().matrix) cb = c->props().matrix->mapRect(cb);
      box = unionRect(box, cb);
    }
  } else if (p.kind == Kind::Text || p.kind == Kind::TSpan) {
    box = scene_.text().outline(e, f, depth).boundingRect();
    if (painted && !box.isNull()) {
      const double w = ShapePainter::strokeWidth(f.style, units);
      if (f.style.stroke.type != Brush::Type::None && w > 0) {
        const double r = strokeReach(f.style, w);
        box.adjust(-r, -r, r, r);
      }
    }
  } else if (p.kind == Kind::Use) {
    if (const auto* ref = scene_.byId(p.href)) {
      QRectF rb = compute(ref, f, depth + 1, painted);
      if (!rb.isNull() && ref->props().matrix) rb = ref->props().matrix->mapRect(rb);
      if (!rb.isNull()) box = rb.translated(units.resolve(p.x, Axis::X), units.resolve(p.y, Axis::Y));
      if (ref->props().kind == Kind::Symbol) {
        const QRectF viewport(units.resolve(p.x, Axis::X), units.resolve(p.y, Axis::Y),
                              units.resolve(p.width, Axis::X, f.viewport.width()),
                              units.resolve(p.height, Axis::Y, f.viewport.height()));
        box = box.isNull() ? QRectF() : box.intersected(viewport);
      }
    }
  } else if (p.kind == Kind::Image) {
    box = QRectF(units.resolve(p.x, Axis::X), units.resolve(p.y, Axis::Y),
                 units.resolve(p.width, Axis::X, p.image.width()), units.resolve(p.height, Axis::Y, p.image.height()));
  }
  if (painted && !p.filter.isEmpty()) {
    if (const auto* fe = scene_.byId(p.filter); fe && fe->props().kind == Kind::Filter) {
      box = unionRect(box, scene_.filters().regionInUserSpace(*fe, f, compute(e, inherited, depth + 1, false)));
    }
  }
  return box;
}

}  // namespace rnqnx::views::rnsvg
