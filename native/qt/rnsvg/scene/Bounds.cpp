#include "Bounds.h"

#include "../RnsvgRenderer.h"
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

QRectF Bounds::nestedBox(const RnsvgRootItem& nested, const Frame& f, int depth, bool painted) {
  if (depth > kMaxDepth) return {};
  const RootProps& rp = nested.rootProps();
  const Units units(f);
  const QSizeF size(units.resolve(rp.bbWidth, Axis::X, nested.width()), units.resolve(rp.bbHeight, Axis::Y, nested.height()));
  if (!(size.width() > 0) || !(size.height() > 0)) return {};
  Frame nf = f;
  nf.viewport = rp.hasViewBox() ? QSizeF(rp.vbWidth, rp.vbHeight) : size;
  const QTransform view = viewTransform(rp, size);
  QRectF box;
  for (QQuickItem* child : nested.childItems()) {
    QRectF cb;
    if (const auto* c = qobject_cast<const RnsvgElementItem*>(child)) {
      cb = compute(c, nf, depth + 1, painted);
      if (!cb.isNull() && c->props().matrix) cb = c->props().matrix->mapRect(cb);
    } else if (const auto* inner = qobject_cast<const RnsvgRootItem*>(child)) {
      cb = nestedBox(*inner, nf, depth + 1, painted);
    }
    if (!cb.isNull()) box = unionRect(box, view.mapRect(cb));
  }
  // The nested viewport clips its content.
  return box.isNull() ? QRectF() : box.intersected(QRectF(QPointF(0, 0), size));
}

QRectF Bounds::object(const RnsvgElementItem* e, const Frame& inherited, int depth) {
  return compute(e, inherited, depth, false);
}

QRectF Bounds::paint(const RnsvgElementItem* e, const Frame& inherited, int depth) {
  return compute(e, inherited, depth, true);
}

QRectF Bounds::compute(const RnsvgElementItem* e, const Frame& inherited, int depth, bool painted) {
  if (depth > kMaxDepth) return {};
  // One key per element and mode: the painted box asks for the object box of
  // the same element (filter regions), which is not a cycle.
  Scene::AnalysisScope scope(scene_, reinterpret_cast<const char*>(e) + (painted ? 1 : 0));
  if (!scope.ok()) return {};
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
    for (QQuickItem* child : e->contentChildren()) {
      if (const auto* c = qobject_cast<const RnsvgElementItem*>(child)) {
        QRectF cb = compute(c, f, depth + 1, painted);
        if (!cb.isNull() && c->props().matrix) cb = c->props().matrix->mapRect(cb);
        box = unionRect(box, cb);
      } else if (const auto* nested = qobject_cast<const RnsvgRootItem*>(child)) {
        box = unionRect(box, nestedBox(*nested, f, depth + 1, painted));
      }
    }
  } else if (p.kind == Kind::Text || p.kind == Kind::TSpan) {
    box = painted ? scene_.text().outline(e, f, depth).boundingRect() : scene_.text().cellBox(e, f, depth);
    if (painted && !box.isNull()) {
      const double w = ShapePainter::strokeWidth(f.style, units);
      if (f.style.stroke.type != Brush::Type::None && w > 0) {
        const double r = strokeReach(f.style, w);
        box.adjust(-r, -r, r, r);
      }
    }
  } else if (p.kind == Kind::Use) {
    if (const auto* ref = scene_.byId(p.href)) {
      const double ux = units.resolve(p.x, Axis::X), uy = units.resolve(p.y, Axis::Y);
      QRectF rb = compute(ref, f, depth + 1, painted);
      if (!rb.isNull() && ref->props().matrix) rb = ref->props().matrix->mapRect(rb);
      if (ref->props().kind == Kind::Symbol) {
        // The symbol's content is in its viewBox space; the use's width and
        // height lay that into a viewport the content is clipped to.
        const double w = units.resolve(p.width, Axis::X, f.viewport.width());
        const double h = units.resolve(p.height, Axis::Y, f.viewport.height());
        if (!rb.isNull()) rb = viewBoxTransform(ref->props().viewBox, QSizeF(w, h)).mapRect(rb);
        const QRectF viewport(0, 0, w, h);
        rb = rb.isNull() ? QRectF() : rb.intersected(viewport);
      }
      if (!rb.isNull()) box = rb.translated(ux, uy);
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
