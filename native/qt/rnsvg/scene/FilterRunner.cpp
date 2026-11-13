#include "FilterRunner.h"

#include "../filter/FilterEngine.h"
#include "ImageRenderer.h"
#include "Scene.h"

#include <QPainter>

namespace rnqnx::views::rnsvg {
namespace {

filter::Definition definitionOf(const RnsvgElementItem& filterElement) {
  const ElementProps& p = filterElement.props();
  filter::Definition def;
  def.region.filterUserSpace = p.filterUserSpace;
  def.region.primitiveUserSpace = p.primitiveUserSpace;
  if (p.x.set) def.region.x = p.x;
  if (p.y.set) def.region.y = p.y;
  if (p.width.set) def.region.width = p.width;
  if (p.height.set) def.region.height = p.height;
  for (auto* child : filterElement.elementChildren()) {
    const ElementProps& cp = child->props();
    if (cp.kind == Kind::FilterPrimitive && cp.primitive) def.primitives.push_back(cp.primitive.get());
  }
  return def;
}

}  // namespace

QRect FilterRunner::region(const RnsvgElementItem& filterElement, const ElementProps&, const Frame& f,
                           const EffectBoxes& boxes) {
  filter::Context ctx;
  ctx.userToDevice = f.ctm * scene_.target().base;
  ctx.objectBoundingBox = boxes.objectBox();
  ctx.viewport = f.viewport;
  ctx.fontSize = f.style.font.size.value_or(12.0);
  return filter::regionInDevice(definitionOf(filterElement), ctx);
}

QRectF FilterRunner::regionInUserSpace(const RnsvgElementItem& filterElement, const Frame& f,
                                       const QRectF& objectBox) {
  const ElementProps& p = filterElement.props();
  const Units u(f);
  const Length defaults[4] = {{-10, Length::Unit::Percent, true},
                              {-10, Length::Unit::Percent, true},
                              {120, Length::Unit::Percent, true},
                              {120, Length::Unit::Percent, true}};
  const Length& lx = p.x.set ? p.x : defaults[0];
  const Length& ly = p.y.set ? p.y : defaults[1];
  const Length& lw = p.width.set ? p.width : defaults[2];
  const Length& lh = p.height.set ? p.height : defaults[3];
  if (p.filterUserSpace) {
    return QRectF(u.resolve(lx, Axis::X), u.resolve(ly, Axis::Y), u.resolve(lw, Axis::X), u.resolve(lh, Axis::Y));
  }
  if (!objectBox.isValid()) return {};
  return QRectF(objectBox.x() + u.resolveUnits(lx, false, Axis::X, 0) * objectBox.width(),
                objectBox.y() + u.resolveUnits(ly, false, Axis::Y, 0) * objectBox.height(),
                u.resolveUnits(lw, false, Axis::X, 1) * objectBox.width(),
                u.resolveUnits(lh, false, Axis::Y, 1) * objectBox.height());
}

void FilterRunner::run(const RnsvgElementItem& filterElement, const ElementProps& target, const Frame& f,
                       const EffectBoxes& boxes, const QRect&, QImage& image, QPoint& origin) {
  filter::Context ctx;
  ctx.userToDevice = f.ctm * scene_.target().base;
  ctx.objectBoundingBox = boxes.objectBox();
  ctx.viewport = f.viewport;
  ctx.fontSize = f.style.font.size.value_or(12.0);
  ctx.currentColor = f.style.color;
  if (f.style.fill.type == Brush::Type::Color) ctx.fillPaint = withAlpha(f.style.fill.color, f.style.fillOpacity);
  if (f.style.stroke.type == Brush::Type::Color) ctx.strokePaint = withAlpha(f.style.stroke.color, f.style.strokeOpacity);
  ctx.loadImage = [](const QString& href) { return ImageRenderer::decodeLocal(href); };
  ctx.renderElement = [this, &f](const QString& id, const QTransform& userToDevice, const QRect& rect) -> QImage {
    const auto* el = scene_.byId(id);
    if (!el) return {};
    QImage out(rect.size(), QImage::Format_ARGB32_Premultiplied);
    if (out.isNull()) return {};
    out.fill(Qt::transparent);
    QPainter painter(&out);
    Scene::configure(painter);
    Target t;
    t.painter = &painter;
    t.clip = QRectF(0, 0, rect.width(), rect.height());
    Frame cf = f;
    cf.ctm = userToDevice * QTransform::fromTranslate(-rect.x(), -rect.y());
    scene_.withoutHit([&] { scene_.withTarget(t, [&] { scene_.render(el, cf, 1); }); });
    return out;
  };
  Q_UNUSED(target);
  const filter::Result result = filter::apply(definitionOf(filterElement), ctx, image);
  if (!result.valid) {
    image.fill(Qt::transparent);  // an invalid filter renders the element as nothing
    return;
  }
  image = result.image;
  origin = result.rect.topLeft();
}

}  // namespace rnqnx::views::rnsvg
