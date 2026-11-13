#include "RnsvgRenderer.h"

#include "scene/Geometry.h"
#include "scene/Scene.h"

#include <QPainter>

namespace rnqnx::views::rnsvg {

QTransform viewTransform(const RootProps& props, const QSizeF& size) {
  ViewBox box;
  box.set = true;
  box.minX = props.minX;
  box.minY = props.minY;
  box.width = props.vbWidth;
  box.height = props.vbHeight;
  box.align = props.align;
  box.meetOrSlice = props.meetOrSlice;
  return viewBoxTransform(box, size);
}

PaintStats paintScene(const RnsvgRootItem& root, QPainter* painter) {
  painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
  Scene scene(root, painter, std::nullopt);
  scene.run();
  return scene.stats();
}

const RnsvgElementItem* hitScene(const RnsvgRootItem& root, const QPointF& local) {
  Scene scene(root, nullptr, local);
  scene.run();
  return scene.hitTarget();
}

}  // namespace rnqnx::views::rnsvg
