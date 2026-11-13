#include "Scene.h"

#include "../RnsvgRenderer.h"
#include "Geometry.h"

#include <algorithm>

namespace rnqnx::views::rnsvg {
namespace {

// Pushes an element on the active stack for the duration of its render so a
// reference cycle (a <Use> or mask reaching its own ancestor) draws nothing
// for the repeat. That also bounds fan-out such as two <Use>s of a group that
// contains them.
class ActiveGuard {
 public:
  ActiveGuard(std::vector<const RnsvgElementItem*>& v, const RnsvgElementItem* e) : v_(v) { v_.push_back(e); }
  ~ActiveGuard() { v_.pop_back(); }
  ActiveGuard(const ActiveGuard&) = delete;
  ActiveGuard& operator=(const ActiveGuard&) = delete;

 private:
  std::vector<const RnsvgElementItem*>& v_;
};

bool definitionOnly(Kind k) {
  switch (k) {
    case Kind::Defs: case Kind::ClipPath: case Kind::Mask: case Kind::LinearGradient:
    case Kind::RadialGradient: case Kind::Pattern: case Kind::Symbol: case Kind::Marker:
    case Kind::Filter: case Kind::FilterPrimitive:
      return true;
    default: return false;
  }
}

}  // namespace

Scene::Scene(const RnsvgRootItem& root, QPainter* painter, std::optional<QPointF> hit)
    : root_(root), hit_(hit), bounds_(*this), paintServers_(*this), shapes_(*this), clipMask_(*this),
      markers_(*this), hitTester_(*this), effects_(*this), text_(*this), images_(*this), filters_(*this) {
  const QSizeF size(root.width(), root.height());
  const auto& rp = root.rootProps();
  view_ = viewTransform(rp, size);
  viewport_ = (rp.vbWidth > 0 && rp.vbHeight > 0) ? QSizeF(rp.vbWidth, rp.vbHeight) : size;
  rootColor_ = rp.color;
  collect(&root, 0);
  target_.painter = painter;
  if (painter) target_.base = painter->transform();
  target_.clip = target_.base.mapRect(QRectF(0, 0, size.width(), size.height()));
}

namespace {

// Element and nested-root children of a root or an element, in paint order.
QList<QQuickItem*> contentOf(const QQuickItem* container) {
  QList<QQuickItem*> out;
  for (auto* child : container->childItems()) {
    if (qobject_cast<RnsvgElementItem*>(child) || qobject_cast<RnsvgRootItem*>(child)) out.append(child);
  }
  return out;
}

}  // namespace

void Scene::collect(const QQuickItem* container, int depth) {
  if (depth > 256) return;
  for (auto* child : contentOf(container)) {
    if (const auto* e = qobject_cast<const RnsvgElementItem*>(child)) {
      if (!e->props().name.isEmpty()) ids_.emplace(e->props().name, e);
    }
    collect(child, depth + 1);
  }
}

const RnsvgElementItem* Scene::byId(const QString& id) const {
  const auto it = ids_.find(id);
  return it == ids_.end() ? nullptr : it->second;
}

Frame Scene::rootFrame() const {
  Frame f;
  f.ctm = view_;
  f.style.color = rootColor_;
  f.viewport = viewport_;
  return f;
}

void Scene::run() {
  const Frame f = rootFrame();
  if (painter()) {
    painter()->save();
    painter()->setClipRect(QRectF(0, 0, root_.width(), root_.height()), Qt::IntersectClip);
  }
  renderChildren(&root_, f, -1);
  if (painter()) painter()->restore();
}

void Scene::renderChildren(const QQuickItem* container, const Frame& f, int depth) {
  for (QQuickItem* child : contentOf(container)) {
    if (const auto* e = qobject_cast<const RnsvgElementItem*>(child)) {
      render(e, f, depth + 1);
    } else if (const auto* nested = qobject_cast<const RnsvgRootItem*>(child)) {
      renderNested(*nested, f, depth + 1);
    }
  }
}

Frame Scene::nestedFrame(const RnsvgRootItem& nested, const Frame& f, QSizeF& size) const {
  const RootProps& rp = nested.rootProps();
  const Units units(f);
  size = QSizeF(units.resolve(rp.bbWidth, Axis::X, nested.width()), units.resolve(rp.bbHeight, Axis::Y, nested.height()));
  Frame nf = f;
  nf.ctm = viewTransform(rp, size) * f.ctm;
  nf.viewport = (rp.vbWidth > 0 && rp.vbHeight > 0) ? QSizeF(rp.vbWidth, rp.vbHeight) : size;
  return nf;
}

void Scene::renderNested(const RnsvgRootItem& nested, const Frame& f, int depth) {
  if (depth > kMaxDepth) return;
  QSizeF size;
  const Frame nf = nestedFrame(nested, f, size);
  if (!(size.width() > 0) || !(size.height() > 0)) return;
  if (hit_) {  // the nested viewport clips its content, for touches as for paint
    bool invertible = false;
    const QTransform inverse = f.ctm.inverted(&invertible);
    if (!invertible || !QRectF(QPointF(0, 0), size).contains(inverse.map(*hit_))) return;
  }
  if (painter()) {
    painter()->save();
    painter()->setTransform(f.ctm * target_.base);
    painter()->setClipRect(QRectF(QPointF(0, 0), size), Qt::IntersectClip);
  }
  renderChildren(&nested, nf, depth);
  if (painter()) painter()->restore();
}

void Scene::walkFrames(const FrameVisitor& visit) { walk(&root_, rootFrame(), 0, visit); }

void Scene::walk(const QQuickItem* container, const Frame& parent, int depth, const FrameVisitor& visit) {
  if (depth > kMaxDepth) return;
  for (QQuickItem* child : contentOf(container)) {
    if (const auto* e = qobject_cast<const RnsvgElementItem*>(child)) {
      const auto& p = e->props();
      if (p.displayNone || definitionOnly(p.kind)) continue;  // definitions draw only when referenced
      Frame own = parent;
      applyProps(own.style, p);
      if (p.matrix) own.ctm = *p.matrix * own.ctm;
      if (visit(e, parent, own)) walk(e, own, depth + 1, visit);
    } else if (const auto* nested = qobject_cast<const RnsvgRootItem*>(child)) {
      QSizeF size;
      const Frame nf = nestedFrame(*nested, parent, size);
      walk(nested, nf, depth + 1, visit);
    }
  }
}

void Scene::render(const RnsvgElementItem* e, const Frame& parent, int depth) {
  if (depth > kMaxDepth || ++visits_ > kMaxVisits) return;
  if (std::find(active_.begin(), active_.end(), e) != active_.end()) return;
  ActiveGuard guard(active_, e);
  const auto& p = e->props();
  if (p.displayNone || definitionOnly(p.kind)) return;

  Frame f = parent;
  applyProps(f.style, p);
  if (p.matrix) f.ctm = *p.matrix * f.ctm;

  struct Responsible {
    Scene& s;
    bool on;
    Responsible(Scene& scene, const RnsvgElementItem* el) : s(scene), on(el->props().responsible) {
      if (on) s.pushResponsible(el);
    }
    ~Responsible() {
      if (on) s.popResponsible();
    }
  } responsible(*this, e);

  if (hitMode() && (!p.clipPath.isEmpty() || !p.mask.isEmpty())) {
    EffectBoxes boxes;
    boxes.objectBox = [&] { return bounds_.object(e, parent, depth); };
    boxes.paintBox = [&] { return bounds_.paint(e, parent, depth); };
    if (!effects_.hitPasses(*e, f, boxes, depth)) return;
  }

  switch (p.kind) {
    case Kind::Path: case Kind::Rect: case Kind::Circle: case Kind::Ellipse: case Kind::Line:
      renderShape(e, f, parent, depth);
      break;
    case Kind::Group: renderGroup(e, f, parent, depth); break;
    case Kind::Use: renderUse(e, f, parent, depth); break;
    case Kind::Text: text_.draw(*e, f, depth); break;
    case Kind::Image: images_.draw(*e, f, depth); break;
    default: break;  // TSpan and TextPath draw as part of their <Text>; ForeignObject is live items
  }
}

void Scene::renderShape(const RnsvgElementItem* e, const Frame& f, const Frame& parent, int depth) {
  const auto& p = e->props();
  const QPainterPath path = shapePath(p, Units(f));
  if (path.isEmpty()) return;
  if (hitMode()) {
    hitTester_.testPath(e, path, f, PointerMode::parse(p.pointerEvents));
    return;
  }
  const QRectF bbox = path.boundingRect();
  EffectBoxes boxes;
  boxes.objectBox = [bbox] { return bbox; };
  boxes.paintBox = [&] { return bounds_.paint(e, parent, depth); };
  const bool both = f.style.fill.type != Brush::Type::None && f.style.stroke.type != Brush::Type::None;
  effects_.paint(*e, f, boxes, both, depth, [&] {
    shapes_.draw(path, f, bbox);
    markers_.draw(*e, f, bbox, depth);
  });
}

void Scene::renderGroup(const RnsvgElementItem* e, const Frame& f, const Frame& parent, int depth) {
  const auto& p = e->props();
  if (hitMode()) {
    const PointerMode mode = PointerMode::parse(p.pointerEvents);
    if (mode.region == PointerMode::Region::Box) {
      QPainterPath box;
      box.addRect(bounds_.object(e, parent, depth));
      hitTester_.testPath(e, box, f, mode);
    }
    if (mode.descend) renderChildren(e, f, depth);
    return;
  }
  EffectBoxes boxes;
  boxes.objectBox = [&] { return bounds_.object(e, parent, depth); };
  boxes.paintBox = [&] { return bounds_.paint(e, parent, depth); };
  // Each child carries the full transform itself, so the children render with
  // `f` whatever effect layer is active.
  effects_.paint(*e, f, boxes, true, depth, [&] { renderChildren(e, f, depth); });
}

void Scene::renderUse(const RnsvgElementItem* e, const Frame& f, const Frame& parent, int depth) {
  const auto& p = e->props();
  const auto* ref = byId(p.href);
  if (!ref) return;
  const Units units(f);
  const double x = units.resolve(p.x, Axis::X), y = units.resolve(p.y, Axis::Y);
  Frame uf = f;
  uf.ctm = QTransform::fromTranslate(x, y) * f.ctm;
  const auto& rp = ref->props();
  if (hitMode()) {
    if (rp.kind == Kind::Symbol) {
      // Content outside the symbol's viewport is clipped, so it is not a target.
      const double w = units.resolve(p.width, Axis::X, uf.viewport.width());
      const double h = units.resolve(p.height, Axis::Y, uf.viewport.height());
      bool invertible = false;
      const QTransform inverse = uf.ctm.inverted(&invertible);
      if (!invertible || !hit_ || !QRectF(0, 0, w, h).contains(inverse.map(*hit_))) return;
      renderChildren(ref, symbolFrame(*ref, uf, units, p), depth);
    } else {
      render(ref, uf, depth + 1);
    }
    return;
  }
  EffectBoxes boxes;
  boxes.objectBox = [&] { return bounds_.object(e, parent, depth); };
  boxes.paintBox = [&] { return bounds_.paint(e, parent, depth); };
  effects_.paint(*e, f, boxes, true, depth, [&] {
    pushContext({f.style.fill, f.style.stroke, bounds_.object(ref, uf, depth + 1), f});
    if (rp.kind == Kind::Symbol) {
      renderSymbol(*ref, uf, units, p, depth);
    } else {
      render(ref, uf, depth + 1);
    }
    popContext();
  });
}

}  // namespace rnqnx::views::rnsvg

namespace rnqnx::views::rnsvg {

Frame Scene::symbolFrame(const RnsvgElementItem& symbol, const Frame& useFrame, const Units& units,
                         const ElementProps& use) const {
  const ElementProps& sp = symbol.props();
  const double w = units.resolve(use.width, Axis::X, useFrame.viewport.width());
  const double h = units.resolve(use.height, Axis::Y, useFrame.viewport.height());
  Frame sf = useFrame;
  applyProps(sf.style, sp);
  sf.ctm = viewBoxTransform(sp.viewBox, QSizeF(w, h)) * useFrame.ctm;
  sf.viewport = sp.viewBox.set ? QSizeF(sp.viewBox.width, sp.viewBox.height) : QSizeF(w, h);
  return sf;
}

void Scene::renderSymbol(const RnsvgElementItem& symbol, const Frame& useFrame, const Units& units,
                         const ElementProps& use, int depth) {
  const double w = units.resolve(use.width, Axis::X, useFrame.viewport.width());
  const double h = units.resolve(use.height, Axis::Y, useFrame.viewport.height());
  if (!(w > 0) || !(h > 0)) return;
  QPainter* p = painter();
  p->save();
  p->setTransform(useFrame.ctm * target_.base);
  p->setClipRect(QRectF(0, 0, w, h), Qt::IntersectClip);
  renderChildren(&symbol, symbolFrame(symbol, useFrame, units, use), depth);
  p->restore();
}

}  // namespace rnqnx::views::rnsvg
