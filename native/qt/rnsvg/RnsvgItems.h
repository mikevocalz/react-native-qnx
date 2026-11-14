#pragma once

#include "RnsvgModel.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QSet>

namespace rnqnx::views {

class RnsvgRootItem;

// One react-native-svg element. It is a QQuickItem only so Fabric's ordinary
// create/insert/remove/delete mutations can mount it in the tree; it draws
// nothing. It stays visible (Qt hides the children of an invisible item, and a
// <ForeignObject> nests live views under these) and the touch dispatcher looks
// through it. The root paints the whole retained tree.
class RnsvgElementItem : public QQuickItem {
  Q_OBJECT

 public:
  explicit RnsvgElementItem(rnsvg::Kind kind, QQuickItem* parent = nullptr);
  ~RnsvgElementItem() override;

  rnsvg::Kind kind() const { return props_.kind; }
  const rnsvg::ElementProps& props() const { return props_; }
  // Replaces the retained props and repaints the owning root. No remount.
  void setProps(rnsvg::ElementProps props);
  // Element children in paint order.
  QList<RnsvgElementItem*> elementChildren() const;
  // Element children and nested <Svg> roots, in paint order.
  QList<QQuickItem*> contentChildren() const;
  // False when `pointerEvents` (none, box-only) or display:none keeps touches
  // from reaching the live views under this element.
  bool passesTouchesToChildren() const;
  RnsvgRootItem* root() const;

 protected:
  void itemChange(ItemChange change, const ItemChangeData& data) override;

 private:
  rnsvg::ElementProps props_;
};

// The <Svg> view. Paints the retained element subtree with QPainter into
// Qt's QQuickPaintedItem texture, so the scene graph holds one textured node
// per <Svg> however many elements it contains.
class RnsvgRootItem final : public QQuickPaintedItem {
  Q_OBJECT

 public:
  explicit RnsvgRootItem(QQuickItem* parent = nullptr);
  ~RnsvgRootItem() override;

  const rnsvg::RootProps& rootProps() const { return props_; }
  void setRootProps(const rnsvg::RootProps& props);
  // Requests a repaint after any element change. Cheap and coalesced by Qt.
  void invalidate();
  // Positions the live views of every <ForeignObject> and tells listeners the
  // scene is laid out. Runs from updatePolish() (GUI thread, outside paint());
  // tests call it directly.
  void layoutScene();
  // The painted extent of `element` in this root's coordinates, or an empty
  // rect. Valid after layoutScene(); never call it from paint().
  QRectF elementBounds(const RnsvgElementItem* element) const;
  // Elements whose bounds the owner wants after each layout pass (onSvgLayout
  // reporters). layoutScene() measures all of them in one traversal;
  // reportedBounds() reads that result. Unregistered or unseen: empty rect.
  void watchBounds(const RnsvgElementItem* element);
  void unwatchBounds(const RnsvgElementItem* element);
  QRectF reportedBounds(const RnsvgElementItem* element) const { return reported_.value(element); }
  // Schedules a layout pass (one polish, however many callers) without repainting.
  void requestLayout() { polish(); }
  // A root inside another <Svg>: it paints through the outer root, not itself.
  bool isNested() const;
  // Topmost element with `responsible` under `local` (root item coordinates),
  // or nullptr if the point should fall through to the <Svg> view itself.
  const RnsvgElementItem* hitElement(const QPointF& local) const;
  QList<RnsvgElementItem*> elementChildren() const;
  // Number of paints since construction; lets tests assert update coalescing.
  int paintCount() const { return paintCount_; }
  // Layers allocated by the most recent paint.
  const rnsvg::PaintStats& lastPaintStats() const { return stats_; }

  void paint(QPainter* painter) override;

 Q_SIGNALS:
  // Emitted at the end of layoutScene().
  void sceneLaidOut();

 protected:
  void updatePolish() override;
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
  void itemChange(ItemChange change, const ItemChangeData& data) override;

 private:
  void updateTextureSize();
  rnsvg::RootProps props_;
  int paintCount_{0};
  rnsvg::PaintStats stats_;
  QSet<const RnsvgElementItem*> watched_;
  QHash<const RnsvgElementItem*, QRectF> reported_;
};

namespace rnsvg {
// Creates the Qt item for a react-native-svg Fabric component name
// ("RNSVGSvgView", "RNSVGPath", ...), or nullptr if the name is not one this
// renderer mounts.
QQuickItem* createItemForComponent(const QString& componentName, QQuickItem* parent);
}  // namespace rnsvg

}  // namespace rnqnx::views
