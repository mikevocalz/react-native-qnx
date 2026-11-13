#include "ClipMask.h"

#include "Geometry.h"
#include "Scene.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {

QPainterPath ClipMask::clipPath(const RnsvgElementItem& clip, int elementRule, const QSizeF& viewport, int depth) {
  QPainterPath result;
  if (depth > kMaxDepth) return result;
  Scene::AnalysisScope scope(scene_, &clip);
  if (!scope.ok()) return result;
  Style st;
  applyProps(st, clip.props());
  const Units units(viewport, 12.0);
  for (auto* c : clip.elementChildren()) {
    QPainterPath sub = clipChild(c, st, units, elementRule, depth + 1);
    if (sub.isEmpty()) continue;
    result = result.isEmpty() ? sub : result.united(sub);
  }
  if (!clip.props().clipPath.isEmpty()) {
    // A clipPath can itself be clipped.
    if (const auto* outer = scene_.byId(clip.props().clipPath); outer && outer->props().kind == Kind::ClipPath && outer != &clip) {
      result = result.intersected(clipPath(*outer, -1, viewport, depth + 1));
    }
  }
  if (clip.props().matrix) result = clip.props().matrix->map(result);
  return result;
}

QPainterPath ClipMask::clipChild(const RnsvgElementItem* c, const Style& inherited, const Units& units,
                                 int elementRule, int depth) {
  if (depth > kMaxDepth) return {};
  Scene::AnalysisScope scope(scene_, c);
  if (!scope.ok()) return {};
  const auto& p = c->props();
  if (p.displayNone) return {};
  Style st = inherited;
  applyProps(st, p);
  QPainterPath path;
  if (isShape(p.kind)) {
    path = shapePath(p, Units(units.viewport(), st.font.size.value_or(12.0)));
    // Unset (-1) is evenodd: react-native-svg's iOS and Android views both
    // default clipRule to evenodd, so the same JSX clips the same shape here.
    const int rule = st.clipRule >= 0 ? st.clipRule : elementRule;
    path.setFillRule(rule == 1 ? Qt::WindingFill : Qt::OddEvenFill);
  } else if (p.kind == Kind::Group) {
    for (auto* g : c->elementChildren()) {
      QPainterPath sub = clipChild(g, st, units, elementRule, depth + 1);
      if (sub.isEmpty()) continue;
      path = path.isEmpty() ? sub : path.united(sub);
    }
  } else if (p.kind == Kind::Use) {
    if (const auto* ref = scene_.byId(p.href)) {
      path = clipChild(ref, st, units, elementRule, depth + 1);
      const Units u(units.viewport(), st.font.size.value_or(12.0));
      path = QTransform::fromTranslate(u.resolve(p.x, Axis::X), u.resolve(p.y, Axis::Y)).map(path);
    }
  } else if (p.kind == Kind::Text) {
    Frame frame;
    frame.style = st;
    frame.viewport = units.viewport();
    path = scene_.text().outline(c, frame, depth + 1);
  }
  if (!p.clipPath.isEmpty() && !path.isEmpty()) {
    if (const auto* outer = scene_.byId(p.clipPath); outer && outer->props().kind == Kind::ClipPath) {
      path = path.intersected(clipPath(*outer, -1, units.viewport(), depth + 1));
    }
  }
  if (p.matrix) path = p.matrix->map(path);
  return path;
}

QImage ClipMask::maskImage(const RnsvgElementItem& mask, const QRectF& bbox, const Frame& f, const QRect& rect,
                           int depth) {
  QImage img(rect.size(), QImage::Format_ARGB32_Premultiplied);
  if (img.isNull()) return img;
  img.fill(Qt::transparent);
  const auto& mp = mask.props();
  Frame cf;
  cf.style = f.style;
  applyProps(cf.style, mp);
  cf.viewport = f.viewport;
  const Units units(cf);
  const bool user = mp.maskUserSpace;
  QRectF region;
  if (user) {
    region = QRectF(units.resolve(mp.x, Axis::X, 0), units.resolve(mp.y, Axis::Y, 0),
                    units.resolve(mp.width, Axis::X, cf.viewport.width()),
                    units.resolve(mp.height, Axis::Y, cf.viewport.height()));
  } else {
    region = QRectF(bbox.x() + units.resolveUnits(mp.x, false, Axis::X, -0.1) * bbox.width(),
                    bbox.y() + units.resolveUnits(mp.y, false, Axis::Y, -0.1) * bbox.height(),
                    units.resolveUnits(mp.width, false, Axis::X, 1.2) * bbox.width(),
                    units.resolveUnits(mp.height, false, Axis::Y, 1.2) * bbox.height());
  }
  cf.ctm = f.ctm;
  if (!mp.maskContentUserSpace) cf.ctm = QTransform(bbox.width(), 0, 0, bbox.height(), bbox.x(), bbox.y()) * f.ctm;
  if (mp.matrix) cf.ctm = *mp.matrix * cf.ctm;
  {
    QPainter painter(&img);
    Scene::configure(painter);
    Target target;
    target.painter = &painter;
    target.base = scene_.target().base * QTransform::fromTranslate(-rect.x(), -rect.y());
    target.clip = QRectF(0, 0, rect.width(), rect.height());
    painter.setTransform(f.ctm * target.base);
    painter.setClipRect(region, Qt::IntersectClip);
    scene_.withoutHit([&] { scene_.withTarget(target, [&] { scene_.renderChildren(&mask, cf, depth + 1); }); });
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

}  // namespace rnqnx::views::rnsvg
