#pragma once

#include "RnsvgModel.h"

#include <QList>
#include <QPointF>
#include <QQuickItem>
#include <QQuickPaintedItem>

namespace rnqnx::views {

class RnsvgRootItem;

// One react-native-svg element. It is a QQuickItem only so Fabric's ordinary
// create/insert/remove/delete mutations can mount it in the tree; it draws
// nothing and is never visible. The root paints the whole retained tree.
class RnsvgElementItem final : public QQuickItem {
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
  // Topmost element with `responsible` under `local` (root item coordinates),
  // or nullptr if the point should fall through to the <Svg> view itself.
  const RnsvgElementItem* hitElement(const QPointF& local) const;
  QList<RnsvgElementItem*> elementChildren() const;
  // Number of paints since construction; lets tests assert update coalescing.
  int paintCount() const { return paintCount_; }

  void paint(QPainter* painter) override;

 protected:
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
  void itemChange(ItemChange change, const ItemChangeData& data) override;

 private:
  void updateTextureSize();
  rnsvg::RootProps props_;
  int paintCount_{0};
};

namespace rnsvg {
// Creates the Qt item for a react-native-svg Fabric component name
// ("RNSVGSvgView", "RNSVGPath", ...), or nullptr if the name is not one this
// renderer mounts. Pattern, Symbol, Marker, TextPath, Filter, Fe* and
// ForeignObject are not.
QQuickItem* createItemForComponent(const QString& componentName, QQuickItem* parent);
}  // namespace rnsvg

}  // namespace rnqnx::views
