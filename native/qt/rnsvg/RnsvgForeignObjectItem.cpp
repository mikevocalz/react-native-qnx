#include "RnsvgForeignObjectItem.h"

#include <QMatrix4x4>
#include <QQuickTransform>

#include <algorithm>
#include <cmath>

namespace rnqnx::views {
namespace {

// A matrix this large cannot place anything on screen and overflows the
// single-precision QMatrix4x4 the scene graph uses.
constexpr double kMaxMatrixEntry = 1.0e12;
constexpr double kMinDeterminant = 1.0e-12;
constexpr double kMinBoxSide = 1.0e-6;
constexpr double kMaxBoxSide = 1.0e7;

bool usable(const QTransform& t) {
  const double entries[] = {t.m11(), t.m12(), t.m21(), t.m22(), t.dx(), t.dy()};
  for (const double v : entries) {
    if (!std::isfinite(v) || std::fabs(v) > kMaxMatrixEntry) return false;
  }
  return std::fabs(t.determinant()) >= kMinDeterminant;
}

}  // namespace

// Qt composes an item's transforms after its own (x, y) translation, so the
// matrix applied here is the requested transform with the item's position
// taken back out. Without that, a Yoga position other than (0, 0) (a second
// ForeignObject in a group, say) would shift the box.
class RnsvgForeignObjectItem::Placement final : public QQuickTransform {
 public:
  explicit Placement(QObject* parent) : QQuickTransform(parent) {}

  void set(const QTransform& t) {
    if (t == matrix_) return;
    matrix_ = t;
    update();
  }
  void applyTo(QMatrix4x4* matrix) const override { *matrix *= QMatrix4x4(matrix_); }

 private:
  QTransform matrix_;
};

RnsvgForeignObjectItem::RnsvgForeignObjectItem(QQuickItem* parent)
    : RnsvgElementItem(rnsvg::Kind::ForeignObject, parent), transform_(new Placement(this)) {
  setClip(true);
  setVisible(false);  // until the scene places it
  transform_->appendToItem(this);
}

RnsvgForeignObjectItem::~RnsvgForeignObjectItem() = default;

void RnsvgForeignObjectItem::setPlacement(const QTransform& itemTransform, const QSizeF& boxSize) {
  const bool boxOk = std::isfinite(boxSize.width()) && std::isfinite(boxSize.height()) &&
                     boxSize.width() >= kMinBoxSide && boxSize.height() >= kMinBoxSide;
  if (!boxOk || !usable(itemTransform)) {
    clearPlacement();
    return;
  }
  itemTransform_ = itemTransform;
  box_ = QSizeF(std::min(boxSize.width(), kMaxBoxSide), std::min(boxSize.height(), kMaxBoxSide));
  placed_ = true;
  apply();
}

void RnsvgForeignObjectItem::clearPlacement() {
  placed_ = false;
  itemTransform_ = QTransform();
  box_ = QSizeF();
  apply();
}

void RnsvgForeignObjectItem::apply() {
  if (!placed_) {
    setVisible(false);
    return;
  }
  transform_->set(itemTransform_ * QTransform::fromTranslate(-x(), -y()));
  // The clip is the item's own rectangle, and Yoga sizes the item to its flex
  // box, not to the SVG box. The children's layout does not depend on the
  // item's Qt size, so the size is pinned to the box.
  if (size() != box_) {
    applying_ = true;
    setSize(box_);
    applying_ = false;
  }
  setVisible(true);
}

void RnsvgForeignObjectItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  RnsvgElementItem::geometryChange(newGeometry, oldGeometry);
  if (applying_ || !placed_) return;
  // The layer sets x, y, width and height on every layout pass.
  apply();
}

}  // namespace rnqnx::views
