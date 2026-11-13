#include "RnsvgItems.h"
#include "RnsvgForeignObjectItem.h"
#include "RnsvgRenderer.h"
#include "scene/HitTester.h"
#include "scene/SceneLayout.h"

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
}

RnsvgElementItem::~RnsvgElementItem() = default;

void RnsvgElementItem::setProps(rnsvg::ElementProps props) {
  props.kind = props_.kind;
  props_ = std::move(props);
  if (auto* r = root()) r->invalidate();
}

QList<RnsvgElementItem*> RnsvgElementItem::elementChildren() const { return elementsOf(this); }

QList<QQuickItem*> RnsvgElementItem::contentChildren() const {
  QList<QQuickItem*> out;
  for (auto* child : childItems()) {
    if (qobject_cast<RnsvgElementItem*>(child) || qobject_cast<RnsvgRootItem*>(child)) out.append(child);
  }
  return out;
}

bool RnsvgElementItem::passesTouchesToChildren() const {
  return !props_.displayNone && rnsvg::PointerMode::parse(props_.pointerEvents).descend;
}

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
  // Created with its parent: itemChange() does not run for that, so decide here.
  setVisible(!isNested());
}

RnsvgRootItem::~RnsvgRootItem() = default;

void RnsvgRootItem::setRootProps(const rnsvg::RootProps& props) {
  props_ = props;
  invalidate();
}

void RnsvgRootItem::invalidate() {
  // A nested root is hidden and painted into the outermost root's texture.
  RnsvgRootItem* painting = this;
  for (QQuickItem* item = parentItem(); item; item = item->parentItem()) {
    if (auto* r = qobject_cast<RnsvgRootItem*>(item)) painting = r;
  }
  painting->update();
  painting->polish();
}

void RnsvgRootItem::updatePolish() {
  QQuickPaintedItem::updatePolish();
  layoutScene();
}

void RnsvgRootItem::layoutScene() {
  rnsvg::placeForeignObjects(*this);
  reported_ = rnsvg::boundsOfElements(*this, watched_);
  Q_EMIT sceneLaidOut();
}

void RnsvgRootItem::watchBounds(const RnsvgElementItem* element) { watched_.insert(element); }

void RnsvgRootItem::unwatchBounds(const RnsvgElementItem* element) {
  watched_.remove(element);
  reported_.remove(element);
}

QRectF RnsvgRootItem::elementBounds(const RnsvgElementItem* element) const {
  return rnsvg::boundsOfElement(*this, element);
}

bool RnsvgRootItem::isNested() const {
  for (QQuickItem* item = parentItem(); item; item = item->parentItem()) {
    if (qobject_cast<RnsvgElementItem*>(item) || qobject_cast<RnsvgRootItem*>(item)) return true;
  }
  return false;
}

QList<RnsvgElementItem*> RnsvgRootItem::elementChildren() const { return elementsOf(this); }

const RnsvgElementItem* RnsvgRootItem::hitElement(const QPointF& local) const {
  return rnsvg::hitScene(*this, local);
}

// paint() reads the element items' props without locking. That is safe only
// because QQuickPaintedItem::paint runs while Qt holds the GUI thread in the
// scene-graph sync phase (both the Image and FramebufferObject targets), or on
// the GUI thread itself in tests. Never call it from a worker or grab thread.
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
  stats_ = rnsvg::paintScene(*this, painter);
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
  // A direct child leaving the tree has no other hook: the element it was is
  // already detached by the time it could invalidate.
  if (change == ItemChildAddedChange || change == ItemChildRemovedChange) update();
  // A nested <Svg> is painted by the outer root (rnsvg::Scene), never by its own item.
  if (change == ItemParentHasChanged) setVisible(!isNested());
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
  if (name == QStringLiteral("RNSVGForeignObject")) return new RnsvgForeignObjectItem(parent);
  struct Row { const char* name; Kind kind; };
  static const Row rows[] = {
      {"RNSVGGroup", Kind::Group},     {"RNSVGDefs", Kind::Defs},
      {"RNSVGPath", Kind::Path},       {"RNSVGCircle", Kind::Circle},
      {"RNSVGEllipse", Kind::Ellipse}, {"RNSVGRect", Kind::Rect},
      {"RNSVGLine", Kind::Line},       {"RNSVGLinearGradient", Kind::LinearGradient},
      {"RNSVGRadialGradient", Kind::RadialGradient}, {"RNSVGClipPath", Kind::ClipPath},
      {"RNSVGMask", Kind::Mask},       {"RNSVGText", Kind::Text},
      {"RNSVGTSpan", Kind::TSpan},     {"RNSVGUse", Kind::Use},
      {"RNSVGImage", Kind::Image},       {"RNSVGPattern", Kind::Pattern},
      {"RNSVGSymbol", Kind::Symbol},     {"RNSVGMarker", Kind::Marker},
      {"RNSVGTextPath", Kind::TextPath}, {"RNSVGFilter", Kind::Filter},
  };
  for (const auto& row : rows) {
    if (name == QLatin1String(row.name)) return new RnsvgElementItem(row.kind, parent);
  }
  // Every RNSVGFe* component is a filter primitive; its props decide which.
  if (name.startsWith(QStringLiteral("RNSVGFe"))) return new RnsvgElementItem(Kind::FilterPrimitive, parent);
  return nullptr;
}

}  // namespace rnsvg
}  // namespace rnqnx::views
