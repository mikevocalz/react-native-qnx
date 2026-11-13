#include "RnsvgRenderer.h"

#include <QFont>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPen>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>

namespace rnqnx::views::rnsvg {
namespace {

constexpr int kMaxVisits = 200000;    // elements visited per paint or hit test
constexpr int kMaxDepth = 32;           // nesting, <use> chains and mask recursion
constexpr double kTextProbeSize = 100;  // glyph outlines are built at this size, then scaled

// The paint state SVG inherits down the tree.
struct Style {
  Brush fill{Brush::Type::Color, QColor(Qt::black), {}};
  Brush stroke;
  double fillOpacity{1}, strokeOpacity{1};
  bool fillEvenOdd{false};
  Length strokeWidth{1, Length::Unit::Px, true};
  int cap{0}, join{0};
  std::vector<Length> dash;
  double dashOffset{0};
  double miter{4};
  bool nonScaling{false};
  QColor color{Qt::black};
  FontSpec font;
};

struct Frame {
  QTransform ctm;  // user space of the current element to root item space
  Style style;
};

QColor withAlpha(QColor c, double a) {
  c.setAlphaF(std::clamp(c.alphaF() * a, 0.0, 1.0));
  return c;
}

void applyProps(Style& s, const ElementProps& p) {
  const auto& pl = p.propList;
  if (pl.contains(QStringLiteral("fill"))) s.fill = p.fill;
  if (pl.contains(QStringLiteral("fillOpacity"))) s.fillOpacity = p.fillOpacity;
  if (pl.contains(QStringLiteral("fillRule"))) s.fillEvenOdd = p.fillEvenOdd;
  if (pl.contains(QStringLiteral("stroke"))) s.stroke = p.stroke;
  if (pl.contains(QStringLiteral("strokeWidth")) && p.strokeWidth.set) s.strokeWidth = p.strokeWidth;
  if (pl.contains(QStringLiteral("strokeOpacity"))) s.strokeOpacity = p.strokeOpacity;
  if (pl.contains(QStringLiteral("strokeDasharray"))) s.dash = p.strokeDasharray;
  if (pl.contains(QStringLiteral("strokeDashoffset"))) s.dashOffset = p.strokeDashoffset;
  if (pl.contains(QStringLiteral("strokeLinecap"))) s.cap = p.strokeLinecap;
  if (pl.contains(QStringLiteral("strokeLinejoin"))) s.join = p.strokeLinejoin;
  if (pl.contains(QStringLiteral("strokeMiterlimit")) && p.strokeMiterlimit > 0) s.miter = p.strokeMiterlimit;
  if (p.nonScalingStroke) s.nonScaling = true;
  if (p.color) s.color = *p.color;
  if (!p.font.family.isEmpty()) s.font.family = p.font.family;
  if (p.font.size) s.font.size = p.font.size;
  if (p.font.weight) s.font.weight = p.font.weight;
  if (p.font.italic) s.font.italic = p.font.italic;
  if (!p.font.anchor.isEmpty()) s.font.anchor = p.font.anchor;
  if (p.font.letterSpacing) s.font.letterSpacing = p.font.letterSpacing;
}

QRectF unionRect(const QRectF& a, const QRectF& b) {
  if (a.isNull()) return b;
  if (b.isNull()) return a;
  return a.united(b);
}

class Scene {
 public:
  Scene(const RnsvgRootItem& root, QPainter* painter, std::optional<QPointF> hit)
      : root_(root), painter_(painter), hit_(hit) {
    const QSizeF size(root.width(), root.height());
    const auto& rp = root.rootProps();
    view_ = viewTransform(rp, size);
    viewport_ = (rp.vbWidth > 0 && rp.vbHeight > 0) ? QSizeF(rp.vbWidth, rp.vbHeight) : size;
    rootColor_ = rp.color;
    for (auto* child : root.elementChildren()) collect(child);
    if (painter_) base_ = painter_->transform();
  }

  void run() {
    Frame f;
    f.ctm = view_;
    f.style.color = rootColor_;
    if (painter_) {
      painter_->save();
      painter_->setClipRect(QRectF(0, 0, root_.width(), root_.height()), Qt::IntersectClip);
    }
    for (auto* child : root_.elementChildren()) render(child, f, 0);
    if (painter_) painter_->restore();
  }

  const RnsvgElementItem* hitTarget() const { return hitTarget_; }

 private:
  enum class Axis { X, Y, Diag };

  void collect(const RnsvgElementItem* e, int depth = 0) {
    if (depth > 256) return;
    if (!e->props().name.isEmpty()) ids_.emplace(e->props().name, e);
    for (auto* c : e->elementChildren()) collect(c, depth + 1);
  }

  const RnsvgElementItem* byId(const QString& id) const {
    const auto it = ids_.find(id);
    return it == ids_.end() ? nullptr : it->second;
  }

  double resolve(const Length& l, Axis axis, const Style& st, double fallback = 0) const {
    if (!l.set) return fallback;
    switch (l.unit) {
      case Length::Unit::Percent: {
        const double w = viewport_.width(), h = viewport_.height();
        const double ref = axis == Axis::X ? w : axis == Axis::Y ? h : std::sqrt((w * w + h * h) / 2.0);
        return l.value / 100.0 * ref;
      }
      case Length::Unit::Em: return l.value * st.font.size.value_or(12.0);
      default: return l.value;
    }
  }

  // objectBoundingBox lengths are fractions; percents are fractions too.
  double resolveUnits(const Length& l, bool userSpace, Axis axis, const Style& st, double fallback) const {
    if (!l.set) return fallback;
    if (userSpace) return resolve(l, axis, st);
    return l.unit == Length::Unit::Percent ? l.value / 100.0 : l.value;
  }

  // ---- geometry ---------------------------------------------------------

  QPainterPath shapePath(const ElementProps& p, const Style& st) const {
    QPainterPath path;
    switch (p.kind) {
      case Kind::Path: path = p.path; break;
      case Kind::Rect: {
        const double x = resolve(p.x, Axis::X, st), y = resolve(p.y, Axis::Y, st);
        const double w = resolve(p.width, Axis::X, st), h = resolve(p.height, Axis::Y, st);
        if (w <= 0 || h <= 0) break;
        double rx = p.rx.set ? resolve(p.rx, Axis::X, st) : (p.ry.set ? resolve(p.ry, Axis::Y, st) : 0);
        double ry = p.ry.set ? resolve(p.ry, Axis::Y, st) : rx;
        rx = std::min(std::max(rx, 0.0), w / 2);
        ry = std::min(std::max(ry, 0.0), h / 2);
        if (rx > 0 && ry > 0) path.addRoundedRect(QRectF(x, y, w, h), rx, ry);
        else path.addRect(QRectF(x, y, w, h));
        break;
      }
      case Kind::Circle: {
        const double r = resolve(p.r, Axis::Diag, st);
        if (r <= 0) break;
        path.addEllipse(QPointF(resolve(p.cx, Axis::X, st), resolve(p.cy, Axis::Y, st)), r, r);
        break;
      }
      case Kind::Ellipse: {
        double rx = resolve(p.rx, Axis::X, st), ry = resolve(p.ry, Axis::Y, st);
        if (!p.rx.set) rx = ry;
        if (!p.ry.set) ry = rx;
        if (rx <= 0 || ry <= 0) break;
        path.addEllipse(QPointF(resolve(p.cx, Axis::X, st), resolve(p.cy, Axis::Y, st)), rx, ry);
        break;
      }
      case Kind::Line:
        path.moveTo(resolve(p.x1, Axis::X, st), resolve(p.y1, Axis::Y, st));
        path.lineTo(resolve(p.x2, Axis::X, st), resolve(p.y2, Axis::Y, st));
        break;
      default: break;
    }
    return path;
  }

  static bool isShape(Kind k) {
    return k == Kind::Path || k == Kind::Rect || k == Kind::Circle || k == Kind::Ellipse || k == Kind::Line;
  }

  QRectF bboxOf(const RnsvgElementItem* e, const Style& inherited, int depth) const {
    if (depth > kMaxDepth) return {};
    const auto& p = e->props();
    Style st = inherited;
    applyProps(st, p);
    QRectF box;
    if (isShape(p.kind)) {
      box = shapePath(p, st).boundingRect();
    } else if (p.kind == Kind::Group || p.kind == Kind::Text || p.kind == Kind::TSpan) {
      for (auto* c : e->elementChildren()) {
        QRectF cb = bboxOf(c, st, depth + 1);
        if (!cb.isNull() && c->props().matrix) cb = c->props().matrix->mapRect(cb);
        box = unionRect(box, cb);
      }
    } else if (p.kind == Kind::Use) {
      if (const auto* ref = byId(p.href)) {
        QRectF rb = bboxOf(ref, st, depth + 1);
        if (!rb.isNull() && ref->props().matrix) rb = ref->props().matrix->mapRect(rb);
        if (!rb.isNull()) box = rb.translated(resolve(p.x, Axis::X, st), resolve(p.y, Axis::Y, st));
      }
    } else if (p.kind == Kind::Image) {
      box = QRectF(resolve(p.x, Axis::X, st), resolve(p.y, Axis::Y, st),
                   resolve(p.width, Axis::X, st), resolve(p.height, Axis::Y, st));
    }
    return box;  // in this element's own space, inside its own matrix
  }

  // ---- paint servers ----------------------------------------------------

  static QColor colorOf(const Brush& b, const Style& st) {
    return b.type == Brush::Type::CurrentColor ? st.color : b.color;
  }

  // Returns false when the brush paints nothing (none, or a missing server).
  bool makeBrush(const Brush& b, const QRectF& bbox, const Style& st, double opacity, QBrush& out) const {
    switch (b.type) {
      case Brush::Type::None: return false;
      case Brush::Type::Color:
      case Brush::Type::CurrentColor:
        out = QBrush(withAlpha(colorOf(b, st), opacity));
        return true;
      case Brush::Type::ContextFill:
      case Brush::Type::ContextStroke:
        return false;  // context paint needs <marker>/<use> context, which is unsupported
      case Brush::Type::Ref: {
        const auto* server = byId(b.ref);
        if (!server) return false;
        const auto& g = server->props();
        if (g.kind != Kind::LinearGradient && g.kind != Kind::RadialGradient) return false;
        if (g.stops.empty()) return false;
        const bool user = g.gradientUserSpace;
        if (!user && (bbox.width() <= 0 || bbox.height() <= 0)) return false;
        QGradientStops stops;
        for (const auto& s : g.stops) {
          stops.append({std::clamp(s.offset, 0.0, 1.0), withAlpha(s.color, opacity)});
        }
        QTransform t;
        if (!user) t = QTransform(bbox.width(), 0, 0, bbox.height(), bbox.x(), bbox.y());
        if (g.gradientTransform) t = *g.gradientTransform * t;
        if (g.kind == Kind::LinearGradient) {
          QLinearGradient grad(resolveUnits(g.x1, user, Axis::X, st, 0), resolveUnits(g.y1, user, Axis::Y, st, 0),
                               resolveUnits(g.x2, user, Axis::X, st, user ? viewport_.width() : 1),
                               resolveUnits(g.y2, user, Axis::Y, st, 0));
          grad.setStops(stops);
          QBrush brush(grad);
          brush.setTransform(t);
          out = brush;
          return true;
        }
        const double cx = resolveUnits(g.cx, user, Axis::X, st, user ? viewport_.width() / 2 : 0.5);
        const double cy = resolveUnits(g.cy, user, Axis::Y, st, user ? viewport_.height() / 2 : 0.5);
        double rx = resolveUnits(g.rx, user, Axis::X, st, user ? viewport_.width() / 2 : 0.5);
        double ry = resolveUnits(g.ry, user, Axis::Y, st, user ? viewport_.height() / 2 : 0.5);
        if (rx <= 0 || ry <= 0) return false;
        const double fx = resolveUnits(g.fx, user, Axis::X, st, cx);
        const double fy = resolveUnits(g.fy, user, Axis::Y, st, cy);
        // An elliptical gradient is a circular one scaled about its center.
        QRadialGradient grad(QPointF(cx, cy), rx, QPointF(fx, cy + (fy - cy) * rx / ry));
        grad.setStops(stops);
        QBrush brush(grad);
        brush.setTransform(QTransform::fromTranslate(0, cy).scale(1, ry / rx).translate(0, -cy) * t);
        out = brush;
        return true;
      }
    }
    return false;
  }

  // ---- clip and mask ----------------------------------------------------

  QPainterPath clipOf(const RnsvgElementItem* clip, int depth) const {
    QPainterPath result;
    if (depth > kMaxDepth) return result;
    Style st;
    applyProps(st, clip->props());
    for (auto* c : clip->elementChildren()) {
      QPainterPath sub = clipChild(c, st, depth + 1);
      if (sub.isEmpty()) continue;
      result = result.isEmpty() ? sub : result.united(sub);
    }
    if (clip->props().matrix) result = clip->props().matrix->map(result);
    return result;
  }

  QPainterPath clipChild(const RnsvgElementItem* c, const Style& inherited, int depth) const {
    if (depth > kMaxDepth) return {};
    const auto& p = c->props();
    Style st = inherited;
    applyProps(st, p);
    QPainterPath path;
    if (isShape(p.kind)) {
      path = shapePath(p, st);
      // clipRule is 0 both when evenodd was asked for and when it was left
      // unset (the generated default), so only nonzero is honored.
      path.setFillRule(Qt::WindingFill);
    } else if (p.kind == Kind::Group) {
      for (auto* g : c->elementChildren()) {
        QPainterPath sub = clipChild(g, st, depth + 1);
        path = path.isEmpty() ? sub : path.united(sub);
      }
    } else if (p.kind == Kind::Use) {
      if (const auto* ref = byId(p.href)) {
        path = clipChild(ref, st, depth + 1);
        path = QTransform::fromTranslate(resolve(p.x, Axis::X, st), resolve(p.y, Axis::Y, st)).map(path);
      }
    } else if (p.kind == Kind::Text || p.kind == Kind::TSpan) {
      // Text outlines as clip geometry are not supported.
      return {};
    }
    if (p.matrix) path = p.matrix->map(path);
    return path;
  }

  // Luminance (or alpha) of the mask content, as an alpha-only image.
  QImage maskImage(const RnsvgElementItem* mask, const QRectF& bbox, const Frame& f, const QSize& size, int depth) {
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    const auto& mp = mask->props();
    Style st = f.style;
    applyProps(st, mp);
    {
      QPainter mp2(&img);
      configure(mp2);
      QPainter* saved = painter_;
      painter_ = &mp2;
      mp2.setTransform(f.ctm * base_);
      // Mask region.
      const bool user = mp.maskUserSpace;
      QRectF region;
      if (user) {
        region = QRectF(resolve(mp.x, Axis::X, st), resolve(mp.y, Axis::Y, st),
                        resolve(mp.width, Axis::X, st, viewport_.width()),
                        resolve(mp.height, Axis::Y, st, viewport_.height()));
      } else {
        region = QRectF(bbox.x() + resolveUnits(mp.x, false, Axis::X, st, -0.1) * bbox.width(),
                        bbox.y() + resolveUnits(mp.y, false, Axis::Y, st, -0.1) * bbox.height(),
                        resolveUnits(mp.width, false, Axis::X, st, 1.2) * bbox.width(),
                        resolveUnits(mp.height, false, Axis::Y, st, 1.2) * bbox.height());
      }
      mp2.setClipRect(region, Qt::IntersectClip);
      Frame cf;
      cf.ctm = f.ctm;
      cf.style = Style();
      cf.style.color = f.style.color;
      if (!mp.maskContentUserSpace) cf.ctm = QTransform(bbox.width(), 0, 0, bbox.height(), bbox.x(), bbox.y()) * f.ctm;
      if (mp.matrix) cf.ctm = *mp.matrix * cf.ctm;
      for (auto* c : mask->elementChildren()) render(c, cf, depth + 1);
      painter_ = saved;
    }
    if (!mp.maskAlpha) {
      for (int y = 0; y < img.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
          // Premultiplied channels already carry the alpha factor.
          const QRgb px = line[x];
          const double lum = 0.2125 * qRed(px) + 0.7154 * qGreen(px) + 0.0721 * qBlue(px);
          line[x] = qRgba(0, 0, 0, std::clamp(int(std::lround(lum)), 0, 255));
        }
      }
    }
    return img;
  }

  // ---- painting ---------------------------------------------------------

  static void configure(QPainter& p) {
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
  }

  QPen makePen(const QBrush& brush, const Style& st) const {
    double w = resolve(st.strokeWidth, Axis::Diag, st, 1);
    QPen pen(brush, w);
    pen.setCapStyle(st.cap == 1 ? Qt::RoundCap : st.cap == 2 ? Qt::SquareCap : Qt::FlatCap);
    pen.setJoinStyle(st.join == 1 ? Qt::RoundJoin : st.join == 2 ? Qt::BevelJoin : Qt::MiterJoin);
    pen.setMiterLimit(st.miter);
    if (st.nonScaling) pen.setCosmetic(true);
    if (!st.dash.empty() && w > 0) {
      QList<qreal> pattern;
      double sum = 0;
      for (const auto& d : st.dash) {
        const double v = resolve(d, Axis::Diag, st);
        pattern.append(std::max(v, 0.0) / w);
        sum += std::max(v, 0.0);
      }
      if (sum > 0 && sum >= 0.05 * std::max(w, 1.0)) {  // tiny dashes explode into geometry
        if (pattern.size() % 2) pattern += pattern;
        pen.setDashPattern(pattern);
        pen.setDashOffset(st.dashOffset / w);
      }
    }
    return pen;
  }

  void drawPath(const QPainterPath& pathIn, const Style& st, const QRectF& bbox) {
    QPainterPath path = pathIn;
    path.setFillRule(st.fillEvenOdd ? Qt::OddEvenFill : Qt::WindingFill);
    QBrush fill, stroke;
    if (makeBrush(st.fill, bbox, st, st.fillOpacity, fill)) painter_->fillPath(path, fill);
    if (makeBrush(st.stroke, bbox, st, st.strokeOpacity, stroke) &&
        resolve(st.strokeWidth, Axis::Diag, st, 1) > 0) {
      painter_->strokePath(path, makePen(stroke, st));
    }
  }

  void hitPath(const RnsvgElementItem* e, const QPainterPath& pathIn, const Style& st, const QTransform& ctm) {
    if (!hit_) return;
    bool invertible = false;
    const QTransform inv = ctm.inverted(&invertible);
    if (!invertible) return;
    const QPointF pt = inv.map(*hit_);
    QPainterPath path = pathIn;
    path.setFillRule(st.fillEvenOdd ? Qt::OddEvenFill : Qt::WindingFill);
    bool inside = false;
    if (st.fill.type != Brush::Type::None && path.contains(pt)) inside = true;
    const double w = resolve(st.strokeWidth, Axis::Diag, st, 1);
    const double reach = w * std::max(st.miter, 1.0);
    if (!inside && st.stroke.type != Brush::Type::None && w > 0 &&
        path.boundingRect().adjusted(-reach, -reach, reach, reach).contains(pt)) {
      QPainterPathStroker stroker;
      stroker.setWidth(w);
      stroker.setCapStyle(st.cap == 1 ? Qt::RoundCap : st.cap == 2 ? Qt::SquareCap : Qt::FlatCap);
      stroker.setJoinStyle(st.join == 1 ? Qt::RoundJoin : st.join == 2 ? Qt::BevelJoin : Qt::MiterJoin);
      inside = stroker.createStroke(path).contains(pt);
    }
    if (!inside) return;
    const RnsvgElementItem* target = e->props().responsible ? e : nullptr;
    for (auto it = responsible_.rbegin(); !target && it != responsible_.rend(); ++it) target = *it;
    if (target) hitTarget_ = target;  // paint order: the last hit is topmost
  }

  // Runs `draw` with the element's clip, mask and group opacity applied.
  void withEffects(const ElementProps& p, const Frame& f, const QRectF& bbox, bool needsLayer, int depth,
                   const std::function<void()>& draw) {
    const RnsvgElementItem* clip = p.clipPath.isEmpty() ? nullptr : byId(p.clipPath);
    const RnsvgElementItem* mask = p.mask.isEmpty() ? nullptr : byId(p.mask);
    if (mask && mask->props().kind != Kind::Mask) mask = nullptr;
    painter_->save();
    painter_->setTransform(f.ctm * base_);
    if (clip && clip->props().kind == Kind::ClipPath) {
      painter_->setClipPath(clipOf(clip, depth), Qt::IntersectClip);
    }
    const bool layer = mask || (needsLayer && p.opacity < 1.0);
    if (!layer) {
      if (p.opacity < 1.0) painter_->setOpacity(painter_->opacity() * p.opacity);
      draw();
      painter_->restore();
      return;
    }
    QPaintDevice* dev = painter_->device();
    const QSize size(dev->width(), dev->height());
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    if (img.isNull()) {  // out of memory: draw without the group effect
      draw();
      painter_->restore();
      return;
    }
    img.fill(Qt::transparent);
    {
      QPainter lp(&img);
      configure(lp);
      lp.setTransform(painter_->transform());
      if (painter_->hasClipping()) lp.setClipPath(painter_->clipPath(), Qt::ReplaceClip);
      QPainter* saved = painter_;
      painter_ = &lp;
      draw();
      painter_ = saved;
      if (mask) {
        const QImage m = maskImage(mask, bbox, f, size, depth);
        lp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        lp.resetTransform();
        lp.setClipping(false);
        lp.drawImage(0, 0, m);
      }
    }
    painter_->resetTransform();
    painter_->setClipping(false);
    painter_->setOpacity(painter_->opacity() * p.opacity);
    painter_->drawImage(0, 0, img);
    painter_->restore();
  }

  // ---- traversal --------------------------------------------------------

  void render(const RnsvgElementItem* e, const Frame& parent, int depth) {
    if (depth > kMaxDepth || ++visits_ > kMaxVisits) return;
    // A reference cycle (a <Use> or mask reaching its own ancestor) draws
    // nothing for the repeat, which also bounds fan-out like two <Use>s of a
    // group that contains them.
    if (std::find(active_.begin(), active_.end(), e) != active_.end()) return;
    struct ActiveGuard {
      std::vector<const RnsvgElementItem*>& v;
      ActiveGuard(std::vector<const RnsvgElementItem*>& vec, const RnsvgElementItem* e) : v(vec) { v.push_back(e); }
      ~ActiveGuard() { v.pop_back(); }
    } guard(active_, e);
    const auto& p = e->props();
    if (p.displayNone) return;
    switch (p.kind) {
      case Kind::Defs: case Kind::ClipPath: case Kind::Mask:
      case Kind::LinearGradient: case Kind::RadialGradient:
        return;  // definitions only; referenced by id
      default: break;
    }
    Frame f = parent;
    applyProps(f.style, p);
    if (p.matrix) f.ctm = *p.matrix * f.ctm;
    const bool hitMode = hit_.has_value();
    const bool pushed = p.responsible;
    if (pushed) responsible_.push_back(e);

    if (isShape(p.kind)) {
      const QPainterPath path = shapePath(p, f.style);
      if (!path.isEmpty()) {
        if (hitMode) {
          if (p.pointerEvents != QStringLiteral("none")) hitPath(e, path, f.style, f.ctm);
        } else {
          const QRectF bbox = path.boundingRect();
          const bool both = f.style.fill.type != Brush::Type::None && f.style.stroke.type != Brush::Type::None;
          withEffects(p, f, bbox, both, depth, [&] { drawPath(path, f.style, bbox); });
        }
      }
    } else if (p.kind == Kind::Group) {
      if (hitMode) {
        if (p.pointerEvents != QStringLiteral("none"))
          for (auto* c : e->elementChildren()) render(c, f, depth + 1);
      } else {
        const QRectF bbox = p.mask.isEmpty() ? QRectF() : bboxOf(e, parent.style, depth);
        withEffects(p, f, bbox, true, depth, [&] {
          Frame inner = f;
          // withEffects already applied this element's matrix to the painter;
          // children carry the full ctm themselves.
          for (auto* c : e->elementChildren()) render(c, inner, depth + 1);
        });
      }
    } else if (p.kind == Kind::Use) {
      if (const auto* ref = byId(p.href)) {
        Frame uf = f;
        uf.ctm = QTransform::fromTranslate(resolve(p.x, Axis::X, f.style), resolve(p.y, Axis::Y, f.style)) * f.ctm;
        if (hitMode) {
          render(ref, uf, depth + 1);
        } else {
          withEffects(p, f, QRectF(), true, depth, [&] { render(ref, uf, depth + 1); });
        }
      }
    } else if (p.kind == Kind::Text) {
      renderText(e, f, depth);
    } else if (p.kind == Kind::Image) {
      renderImage(e, f, depth);
    }
    if (pushed) responsible_.pop_back();
  }

  QFont probeFont(const Style& st) const {
    QFont font;
    if (!st.font.family.isEmpty()) font.setFamily(st.font.family);
    font.setPixelSize(int(kTextProbeSize));
    font.setWeight(QFont::Weight(std::clamp(st.font.weight.value_or(400), 1, 1000)));
    font.setItalic(st.font.italic.value_or(false));
    font.setHintingPreference(QFont::PreferNoHinting);
    const double size = st.font.size.value_or(12.0);
    if (st.font.letterSpacing && size > 0) {
      font.setLetterSpacing(QFont::AbsoluteSpacing, *st.font.letterSpacing * kTextProbeSize / size);
    }
    return font;
  }

  // The Text/TSpan cursor: the SVG "current text position".
  void renderText(const RnsvgElementItem* e, const Frame& f, int depth) {
    if (depth > kMaxDepth) return;
    const auto& p = e->props();
    if (p.displayNone) return;
    const bool top = p.kind == Kind::Text && !inText_;
    const bool prev = inText_;
    const QPointF prevPos = textPos_;
    if (top) { inText_ = true; textPos_ = QPointF(0, 0); }
    // Absolute positions reset the cursor; relative ones move it.
    if (!p.tx.empty()) textPos_.setX(resolve(p.tx.front(), Axis::X, f.style));
    if (!p.ty.empty()) textPos_.setY(resolve(p.ty.front(), Axis::Y, f.style));
    if (!p.tdx.empty()) textPos_.rx() += resolve(p.tdx.front(), Axis::X, f.style);
    if (!p.tdy.empty()) textPos_.ry() += resolve(p.tdy.front(), Axis::Y, f.style);
    if (!p.content.isEmpty()) {
      const QFont font = probeFont(f.style);
      const double size = f.style.font.size.value_or(12.0);
      const double k = size / kTextProbeSize;
      const QFontMetricsF fm(font);
      const double advance = fm.horizontalAdvance(p.content) * k;
      double x = textPos_.x();
      if (f.style.font.anchor == QStringLiteral("middle")) x -= advance / 2;
      else if (f.style.font.anchor == QStringLiteral("end")) x -= advance;
      QPainterPath outline;
      outline.addText(QPointF(0, 0), font, p.content);
      const QTransform place = QTransform::fromTranslate(x, textPos_.y()).scale(k, k);
      const QPainterPath path = place.map(outline);
      textPos_.rx() += advance;
      if (!path.isEmpty()) {
        if (hit_) {
          if (p.pointerEvents != QStringLiteral("none")) hitPath(e, path, f.style, f.ctm);
        } else {
          const QRectF bbox = path.boundingRect();
          const bool both = f.style.fill.type != Brush::Type::None && f.style.stroke.type != Brush::Type::None;
          withEffects(p, f, bbox, both, depth, [&] { drawPath(path, f.style, bbox); });
        }
      }
    }
    for (auto* c : e->elementChildren()) {
      const auto& cp = c->props();
      if (cp.kind != Kind::Text && cp.kind != Kind::TSpan) continue;
      Frame cf = f;
      applyProps(cf.style, cp);
      if (cp.matrix) cf.ctm = *cp.matrix * cf.ctm;
      if (cp.responsible) responsible_.push_back(c);
      renderText(c, cf, depth + 1);
      if (cp.responsible) responsible_.pop_back();
    }
    if (top) { inText_ = prev; textPos_ = prevPos; }
  }

  void renderImage(const RnsvgElementItem* e, const Frame& f, int depth) {
    const auto& p = e->props();
    if (p.image.isNull()) return;
    const QRectF box(resolve(p.x, Axis::X, f.style), resolve(p.y, Axis::Y, f.style),
                     resolve(p.width, Axis::X, f.style, p.image.width()),
                     resolve(p.height, Axis::Y, f.style, p.image.height()));
    if (box.width() <= 0 || box.height() <= 0) return;
    // preserveAspectRatio for the image inside its box.
    const QSizeF src = p.image.size();
    QRectF dest = box;
    QRectF source(QPointF(0, 0), src);
    if (p.imageMeetOrSlice != 2 && p.imageAlign != QStringLiteral("none")) {
      const double sx = box.width() / src.width(), sy = box.height() / src.height();
      const double s = p.imageMeetOrSlice == 1 ? std::max(sx, sy) : std::min(sx, sy);
      const QSizeF scaled = src * s;
      const QString& a = p.imageAlign;
      const double fx = a.startsWith(QStringLiteral("xMin")) ? 0 : a.startsWith(QStringLiteral("xMax")) ? 1 : 0.5;
      const double fy = a.endsWith(QStringLiteral("YMin")) ? 0 : a.endsWith(QStringLiteral("YMax")) ? 1 : 0.5;
      const QRectF fitted(box.x() + (box.width() - scaled.width()) * fx,
                          box.y() + (box.height() - scaled.height()) * fy, scaled.width(), scaled.height());
      if (p.imageMeetOrSlice == 1) {
        source = QRectF((fitted.x() < box.x() ? (box.x() - fitted.x()) / s : 0),
                        (fitted.y() < box.y() ? (box.y() - fitted.y()) / s : 0), box.width() / s, box.height() / s);
      } else {
        dest = fitted;
      }
    }
    if (hit_) {
      if (p.pointerEvents == QStringLiteral("none")) return;
      QPainterPath path;
      path.addRect(dest);
      Style st = f.style;
      st.fill = Brush{Brush::Type::Color, Qt::black, {}};
      hitPath(e, path, st, f.ctm);
      return;
    }
    withEffects(p, f, dest, false, depth, [&] { painter_->drawImage(dest, p.image, source); });
  }

  const RnsvgRootItem& root_;
  QPainter* painter_;
  std::optional<QPointF> hit_;
  QTransform view_, base_;
  QSizeF viewport_;
  QColor rootColor_{Qt::black};
  struct QStringHash {
    size_t operator()(const QString& s) const { return qHash(s); }
  };
  std::unordered_map<QString, const RnsvgElementItem*, QStringHash> ids_;
  std::vector<const RnsvgElementItem*> responsible_;
  std::vector<const RnsvgElementItem*> active_;
  int visits_{0};
  const RnsvgElementItem* hitTarget_{nullptr};
  QPointF textPos_;
  bool inText_{false};
};

}  // namespace

QTransform viewTransform(const RootProps& props, const QSizeF& size) {
  if (props.vbWidth <= 0 || props.vbHeight <= 0 || size.width() <= 0 || size.height() <= 0) {
    return QTransform();
  }
  const double sx = size.width() / props.vbWidth, sy = size.height() / props.vbHeight;
  if (props.meetOrSlice == 2 || props.align == QStringLiteral("none")) {
    return QTransform(sx, 0, 0, sy, -props.minX * sx, -props.minY * sy);
  }
  const double s = props.meetOrSlice == 1 ? std::max(sx, sy) : std::min(sx, sy);
  const QString& a = props.align;
  const double fx = a.startsWith(QStringLiteral("xMin")) ? 0 : a.startsWith(QStringLiteral("xMax")) ? 1 : 0.5;
  const double fy = a.endsWith(QStringLiteral("YMin")) ? 0 : a.endsWith(QStringLiteral("YMax")) ? 1 : 0.5;
  const double tx = (size.width() - props.vbWidth * s) * fx - props.minX * s;
  const double ty = (size.height() - props.vbHeight * s) * fy - props.minY * s;
  return QTransform(s, 0, 0, s, tx, ty);
}

void paintScene(const RnsvgRootItem& root, QPainter* painter) {
  painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
  Scene(root, painter, std::nullopt).run();
}

const RnsvgElementItem* hitScene(const RnsvgRootItem& root, const QPointF& local) {
  Scene scene(root, nullptr, local);
  scene.run();
  return scene.hitTarget();
}

}  // namespace rnqnx::views::rnsvg
