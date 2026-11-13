#include "RnsvgItems.h"
#include "RnsvgRenderer.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QPaintDevice>
#include <QPainter>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>

namespace rnqnx::views {
namespace {
constexpr qreal kMaxTexturePixels = 4096;

QList<RnsvgElementItem*> elementsOf(const QQuickItem* parent) {
  QList<RnsvgElementItem*> out;
  for (auto* child : parent->childItems()) {
    if (auto* element = qobject_cast<RnsvgElementItem*>(child)) out.append(element);
  }
  return out;
}
}  // namespace

RnsvgElementItem::RnsvgElementItem(rnsvg::Kind kind, QQuickItem* parent) : QQuickItem(parent) {
  props_.kind = kind;
  setVisible(false);  // draws nothing; keeps generic geometry hit testing off it
}

RnsvgElementItem::~RnsvgElementItem() = default;

void RnsvgElementItem::setProps(rnsvg::ElementProps props) {
  props.kind = props_.kind;
  props_ = std::move(props);
  if (auto* r = root()) r->invalidate();
}

QList<RnsvgElementItem*> RnsvgElementItem::elementChildren() const { return elementsOf(this); }

RnsvgRootItem* RnsvgElementItem::root() const {
  for (QQuickItem* item = parentItem(); item; item = item->parentItem()) {
    if (auto* r = qobject_cast<RnsvgRootItem*>(item)) return r;
  }
  return nullptr;
}

void RnsvgElementItem::itemChange(ItemChange change, const ItemChangeData& data) {
  QQuickItem::itemChange(change, data);
  if (change == ItemChildAddedChange || change == ItemChildRemovedChange) {
    if (auto* r = root()) r->invalidate();
  } else if (change == ItemParentHasChanged) {
    if (data.item) {
      if (auto* r = root()) r->invalidate();
    }
  }
}

RnsvgRootItem::RnsvgRootItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setOpaquePainting(false);
  // The paint target is a measured choice, see docs/v2/react-native-svg.md.
  // RNQNX_RNSVG_TARGET=fbo|image overrides it for comparison runs.
  const QByteArray target = qgetenv("RNQNX_RNSVG_TARGET");
  if (target == "fbo") setRenderTarget(QQuickPaintedItem::FramebufferObject);
  else if (target == "image") setRenderTarget(QQuickPaintedItem::Image);
  else setRenderTarget(QQuickPaintedItem::Image);
}

RnsvgRootItem::~RnsvgRootItem() = default;

void RnsvgRootItem::setRootProps(const rnsvg::RootProps& props) {
  props_ = props;
  invalidate();
}

void RnsvgRootItem::invalidate() { update(); }

QList<RnsvgElementItem*> RnsvgRootItem::elementChildren() const { return elementsOf(this); }

const RnsvgElementItem* RnsvgRootItem::hitElement(const QPointF& local) const {
  return rnsvg::hitScene(*this, local);
}

void RnsvgRootItem::paint(QPainter* painter) {
  ++paintCount_;
  // RNQNX_RNSVG_TRACE=1 logs how long a full repaint takes: the first five,
  // then every 60th, so a running animation does not flood the log.
  static const bool trace = qEnvironmentVariableIsSet("RNQNX_RNSVG_TRACE");
  QElapsedTimer timer;
  if (trace) timer.start();
  if (!(width() > 0) || !(height() > 0)) return;
  // Fabric applies <Svg style={{backgroundColor}}> as a dynamic property.
  const QVariant background = property("backgroundColor");
  if (background.canConvert<QColor>() && background.value<QColor>().alpha() > 0) {
    painter->fillRect(QRectF(0, 0, width(), height()), background.value<QColor>());
  }
  rnsvg::paintScene(*this, painter);
  if (trace && (paintCount_ <= 5 || paintCount_ % 60 == 0)) {
    qInfo().nospace() << "RNQNX_RNSVG_PAINT n=" << paintCount_ << " ms="
                      << QString::number(timer.nsecsElapsed() / 1e6, 'f', 2) << " size=" << int(width()) << "x"
                      << int(height()) << " device=" << painter->device()->width() << "x"
                      << painter->device()->height();
  }
}

void RnsvgRootItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
  updateTextureSize();
  update();
}

void RnsvgRootItem::itemChange(ItemChange change, const ItemChangeData& data) {
  QQuickPaintedItem::itemChange(change, data);
  if (change == ItemDevicePixelRatioHasChanged || change == ItemSceneChange) updateTextureSize();
}

// Bound the raster the way the static SVG document item does: Qt multiplies
// the texture size by the device pixel ratio.
void RnsvgRootItem::updateTextureSize() {
  const qreal w = width(), h = height();
  if (!std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0) {
    setTextureSize(QSize(1, 1));
    return;
  }
  const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
  const qreal safeDpr = std::isfinite(dpr) && dpr > 0 ? dpr : 1.0;
  const qreal logicalLimit = std::max<qreal>(1.0, std::floor(kMaxTexturePixels / safeDpr));
  const qreal scale = std::min<qreal>(1.0, logicalLimit / std::max(w, h));
  setTextureSize(QSize(std::clamp(int(std::ceil(w * scale)), 1, int(logicalLimit)),
                       std::clamp(int(std::ceil(h * scale)), 1, int(logicalLimit))));
}

namespace rnsvg {

QQuickItem* createItemForComponent(const QString& name, QQuickItem* parent) {
  if (name == QStringLiteral("RNSVGSvgView")) return new RnsvgRootItem(parent);
  struct Row { const char* name; Kind kind; };
  static const Row rows[] = {
      {"RNSVGGroup", Kind::Group},     {"RNSVGDefs", Kind::Defs},
      {"RNSVGPath", Kind::Path},       {"RNSVGCircle", Kind::Circle},
      {"RNSVGEllipse", Kind::Ellipse}, {"RNSVGRect", Kind::Rect},
      {"RNSVGLine", Kind::Line},       {"RNSVGLinearGradient", Kind::LinearGradient},
      {"RNSVGRadialGradient", Kind::RadialGradient}, {"RNSVGClipPath", Kind::ClipPath},
      {"RNSVGMask", Kind::Mask},       {"RNSVGText", Kind::Text},
      {"RNSVGTSpan", Kind::TSpan},     {"RNSVGUse", Kind::Use},
      {"RNSVGImage", Kind::Image},
  };
  for (const auto& row : rows) {
    if (name == QLatin1String(row.name)) return new RnsvgElementItem(row.kind, parent);
  }
  return nullptr;
}

}  // namespace rnsvg
}  // namespace rnqnx::views
