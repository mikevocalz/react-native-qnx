#pragma once

// <ForeignObject>: a box in SVG user space that holds ordinary React Native
// views. Fabric mounts those views as child items of this item, laid out by
// Yoga inside it, exactly as under any other view. This item places itself on
// the <Svg> from a transform the root's scene computes, and clips its children
// to the box.
//
// z-order. The <Svg> paints its elements into one texture on its own item;
// this item's children are live Qt Quick items. Live children always draw
// above that texture, so a painted element that follows the <ForeignObject> in
// document order still appears behind it. Fixing that means splitting the
// painted layer around each <ForeignObject> into several textures; that is a
// separate change and is not attempted here.

#include "RnsvgItems.h"

#include <QSizeF>
#include <QTransform>

namespace rnqnx::views {

class RnsvgForeignObjectItem : public RnsvgElementItem {
  Q_OBJECT

 public:
  explicit RnsvgForeignObjectItem(QQuickItem* parent = nullptr);
  ~RnsvgForeignObjectItem() override;

  // `itemTransform` maps a point written in the box's own coordinates (origin
  // at the box's top-left corner, in the units `boxSize` is measured in) to the
  // coordinates of this item's parent item. The scene composes viewBox, every
  // ancestor matrix, the element's own matrix and its x and y into it.
  // `boxSize` is the box's width and height in the same units.
  //
  // A transform that is not finite, has a determinant near zero or has an
  // entry beyond +-1e12, and a box that is not at least 1e-6 wide and high,
  // hide the item, as SVG disables rendering for a zero-sized foreignObject.
  // Call from the GUI thread, outside paint().
  void setPlacement(const QTransform& itemTransform, const QSizeF& boxSize);
  // Hides the item until the next setPlacement(): display:none, or an element
  // the scene does not reach.
  void clearPlacement();

  // True while the item is placed and shown.
  bool isPlaced() const { return placed_; }
  // The box size last accepted by setPlacement(); (0, 0) when not placed.
  QSizeF boxSize() const { return placed_ ? box_ : QSizeF(); }

 protected:
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

 private:
  class Placement;  // QQuickTransform that carries the matrix

  void apply();

  Placement* transform_;
  QTransform itemTransform_;
  QSizeF box_;
  bool placed_{false};
  bool applying_{false};
};

}  // namespace rnqnx::views
