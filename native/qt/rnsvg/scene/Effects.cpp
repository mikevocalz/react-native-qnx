#include "Effects.h"

#include "Scene.h"

#include <QImage>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {
namespace {

// A layer larger than this many pixels is clipped to the visible target.
constexpr qint64 kMaxLayerPixels = 16 * 1024 * 1024;

const RnsvgElementItem* lookup(const Scene& scene, const QString& id, Kind kind) {
  if (id.isEmpty()) return nullptr;
  const auto* e = scene.byId(id);
  return e && e->props().kind == kind ? e : nullptr;
}

QRect alignedOut(const QRectF& r) {
  if (!r.isValid()) return {};
  return r.adjusted(-1, -1, 1, 1).toAlignedRect();
}

}  // namespace

void Effects::paint(const RnsvgElementItem& e, const Frame& f, const EffectBoxes& boxes, bool layerForOpacity,
                    int depth, const std::function<void()>& draw) {
  const auto& p = e.props();
  const auto* clip = lookup(scene_, p.clipPath, Kind::ClipPath);
  const auto* mask = lookup(scene_, p.mask, Kind::Mask);
  const auto* filter = lookup(scene_, p.filter, Kind::Filter);
  QPainter* painter = scene_.painter();
  const Target outer = scene_.target();
  const QTransform toDevice = f.ctm * outer.base;

  painter->save();
  painter->setTransform(toDevice);
  Target inner = outer;
  if (clip) {
    const QPainterPath path = scene_.clipMask().clipPath(*clip, p.clipRule, f.viewport, depth);
    painter->setClipPath(path, Qt::IntersectClip);
    inner.clip = outer.clip.intersected(toDevice.map(path).boundingRect());
  }
  if (inner.clip.isEmpty()) {  // clipped away entirely
    painter->restore();
    return;
  }

  const bool layer = mask || filter || (layerForOpacity && p.opacity < 1.0);
  if (!layer) {
    if (p.opacity < 1.0) painter->setOpacity(painter->opacity() * p.opacity);
    scene_.withTarget(inner, draw);
    painter->restore();
    return;
  }

  // The layer covers what the element paints. A filter works on its whole
  // region, which can reach past the visible area (a blur, an offset), so it
  // is only limited when it would be huge.
  QRect rect;
  if (filter) {
    rect = scene_.filters().region(*filter, p, f, boxes);
  } else {
    rect = alignedOut(toDevice.mapRect(boxes.paintBox()));
  }
  if (rect.isEmpty()) {
    painter->restore();
    return;
  }
  const QRect visible = inner.clip.adjusted(-1, -1, 1, 1).toAlignedRect();
  if (filter) {
    if (qint64(rect.width()) * rect.height() > kMaxLayerPixels) rect = rect.intersected(visible);
  } else {
    rect = rect.intersected(visible);
  }
  if (rect.isEmpty()) {
    painter->restore();
    return;
  }

  QImage image(rect.size(), QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {  // out of memory: draw without the group effect
    scene_.withTarget(inner, draw);
    painter->restore();
    return;
  }
  image.fill(Qt::transparent);
  scene_.noteLayer(image.size());
  {
    QPainter layerPainter(&image);
    Scene::configure(layerPainter);
    Target layerTarget;
    layerTarget.painter = &layerPainter;
    layerTarget.base = outer.base * QTransform::fromTranslate(-rect.x(), -rect.y());
    layerTarget.clip = QRectF(0, 0, rect.width(), rect.height());
    layerPainter.setTransform(f.ctm * layerTarget.base);
    scene_.withTarget(layerTarget, draw);
  }
  QPoint origin = rect.topLeft();
  if (filter) scene_.filters().run(*filter, p, f, boxes, rect, image, origin);
  if (mask) {
    const QImage m = scene_.clipMask().maskImage(*mask, boxes.objectBox(), f, QRect(origin, image.size()), depth);
    if (!m.isNull()) {
      QPainter maskPainter(&image);
      maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
      maskPainter.drawImage(0, 0, m);
    }
  }
  painter->resetTransform();
  painter->setOpacity(painter->opacity() * p.opacity);
  painter->drawImage(origin, image);
  painter->restore();
}

bool Effects::hitPasses(const RnsvgElementItem& e, const Frame& f, const EffectBoxes& boxes, int depth) {
  const auto& p = e.props();
  const auto* clip = lookup(scene_, p.clipPath, Kind::ClipPath);
  const auto* mask = lookup(scene_, p.mask, Kind::Mask);
  if (!clip && !mask) return true;
  const auto& point = scene_.hitPoint();
  if (!point) return true;
  bool invertible = false;
  const QTransform inverse = f.ctm.inverted(&invertible);
  if (!invertible) return false;
  if (clip) {
    const QPainterPath path = scene_.clipMask().clipPath(*clip, p.clipRule, f.viewport, depth);
    if (!path.contains(inverse.map(*point))) return false;
  }
  if (mask) {
    // Sample the mask at the pixel under the point: paint it into a 1x1 image
    // whose origin is that pixel.
    const QRect pixel(int(std::floor(point->x())), int(std::floor(point->y())), 1, 1);
    const QImage m = scene_.clipMask().maskImage(*mask, boxes.objectBox(), f, pixel, depth);
    if (m.isNull() || qAlpha(m.pixel(0, 0)) == 0) return false;
  }
  return true;
}

}  // namespace rnqnx::views::rnsvg
