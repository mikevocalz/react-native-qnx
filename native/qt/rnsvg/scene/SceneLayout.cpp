#include "SceneLayout.h"

#include "../RnsvgForeignObjectItem.h"
#include "Scene.h"

#include <QSet>

namespace rnqnx::views::rnsvg {
namespace {

void collectForeignObjects(QQuickItem* item, QList<RnsvgForeignObjectItem*>& out, int depth) {
  if (depth > 256) return;
  for (auto* child : item->childItems()) {
    if (auto* fo = qobject_cast<RnsvgForeignObjectItem*>(child)) out.append(fo);
    collectForeignObjects(child, out, depth + 1);
  }
}

}  // namespace

void placeForeignObjects(RnsvgRootItem& root) {
  QList<RnsvgForeignObjectItem*> all;
  collectForeignObjects(&root, all, 0);
  if (all.isEmpty()) return;
  Scene scene(root, nullptr, std::nullopt);
  QSet<const RnsvgForeignObjectItem*> placed;
  scene.walkFrames([&](const RnsvgElementItem* e, const Frame&, const Frame& own) {
    if (e->kind() != Kind::ForeignObject) return true;
    auto* fo = const_cast<RnsvgForeignObjectItem*>(static_cast<const RnsvgForeignObjectItem*>(e));
    const ElementProps& p = e->props();
    const Units units(own);
    const QSizeF box(units.resolve(p.width, Axis::X), units.resolve(p.height, Axis::Y));
    QTransform toRoot = QTransform::fromTranslate(units.resolve(p.x, Axis::X), units.resolve(p.y, Axis::Y)) * own.ctm;
    // The item's transform is relative to its parent item, which Fabric lays
    // out at its own offset inside the root.
    if (QQuickItem* parent = fo->parentItem()) {
      const QPointF origin = parent->mapToItem(&root, QPointF(0, 0));
      toRoot = toRoot * QTransform::fromTranslate(-origin.x(), -origin.y());
    }
    fo->setPlacement(toRoot, box);
    placed.insert(fo);
    return false;  // its children are live views, not SVG elements
  });
  for (auto* fo : all) {
    if (!placed.contains(fo)) fo->clearPlacement();
  }
}

QRectF boundsOfElement(const RnsvgRootItem& root, const RnsvgElementItem* element) {
  Scene scene(root, nullptr, std::nullopt);
  QRectF result;
  bool found = false;
  scene.walkFrames([&](const RnsvgElementItem* e, const Frame& parent, const Frame& own) {
    if (found) return false;
    if (e != element) return true;
    found = true;
    const QRectF box = scene.bounds().paint(e, parent, 0);
    if (!box.isNull()) result = own.ctm.mapRect(box);
    return false;
  });
  return result;
}

}  // namespace rnqnx::views::rnsvg
