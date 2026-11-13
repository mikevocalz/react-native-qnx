#include "QtSvgItem.h"
#include "SvgDocumentValidation.h"

#include <QByteArray>
#include <QPainter>
#include <QEvent>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QSvgRenderer>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <variant>

namespace rnqnx::views {
namespace {
struct ReadyDocument {
  std::unique_ptr<QSvgRenderer> renderer;
  SvgDocumentInfo info;
};
struct RejectedDocument {
  QString message;
};
}  // namespace

struct QtSvgItem::State {
  QByteArray source;
  std::variant<std::monostate, ReadyDocument, RejectedDocument> document;
};

QtSvgItem::QtSvgItem(QQuickItem* parent)
    : QQuickPaintedItem(parent),
      owner_(QCoreApplication::instance() ? QCoreApplication::instance()->thread() : nullptr),
      state_(std::make_unique<State>()) {
  if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
    throw std::logic_error("QtSvgItem requires a QGuiApplication");
  }
  requireOwner();
  setAntialiasing(true);
  setClip(true);
  setFillColor(Qt::transparent);
  setTextureSize(QSize(1, 1));
  connect(this, &QQuickItem::windowChanged, this, [this] { updateTextureSize(); });
}

QtSvgItem::~QtSvgItem() = default;

void QtSvgItem::requireOwner() const {
  if (affinityLost_ || QThread::currentThread() != owner_ || thread() != owner_) {
    throw std::logic_error("QtSvgItem must be used on its Qt GUI owner thread");
  }
}

bool QtSvgItem::event(QEvent* event) {
  if (event->type() == QEvent::ThreadChange && state_ && !affinityLost_) {
    // QObject sends this before changing affinity. Release the GUI-owned
    // renderer here and permanently reject reuse, including a later move back.
    affinityLost_ = true;
    state_->source.clear();
    state_->document = RejectedDocument{QStringLiteral("QtSvgItem GUI thread affinity changed")};
    setTextureSize(QSize(1, 1));
    update();
  }
  return QQuickPaintedItem::event(event);
}

bool QtSvgItem::setDocument(QByteArrayView utf8Xml) {
  requireOwner();
  if (utf8Xml.isEmpty()) {
    clearDocument();
    return true;
  }
  if (utf8Xml.size() <= kSvgMaxDocumentBytes && QByteArrayView(state_->source) == utf8Xml) {
    return std::holds_alternative<ReadyDocument>(state_->document);
  }

  std::optional<QString> error;
  QByteArray ownedSource;
  if (utf8Xml.size() > kSvgMaxDocumentBytes) {
    // Enforce the budget before reading or copying the caller's bytes. Even
    // rejected input arrives as a view so adapters need no preliminary copy.
    error = QStringLiteral("SVG source exceeds 65536 bytes");
  } else {
    // QByteArray::fromRawData can borrow external storage; constructing from
    // the view's pointer and size guarantees our own copy before parsing.
    ownedSource = QByteArray(utf8Xml.data(), utf8Xml.size());
    error = detail::validateSvgDocument(ownedSource);
  }
  std::unique_ptr<QSvgRenderer> renderer;
  SvgDocumentInfo info;
  if (!error) {
    renderer = std::make_unique<QSvgRenderer>();
    // Set explicit per-instance options; ambient QtSvg defaults must not widen
    // this component's contract. No resource-loading or animation elements pass
    // validation, so the renderer never owns a live animation producer.
    renderer->setOptions(QtSvg::Tiny12FeaturesOnly);
    renderer->setAnimationEnabled(false);
    if (!renderer->load(ownedSource) || !renderer->isValid()) {
      error = QStringLiteral("Qt SVG could not parse the static document");
    } else {
      renderer->setAnimationEnabled(false);
      renderer->setAspectRatioMode(Qt::IgnoreAspectRatio);
      info = {QSizeF(renderer->defaultSize()), renderer->viewBoxF()};
      const auto viewBox = info.viewBox;
      if (!std::isfinite(viewBox.x()) || !std::isfinite(viewBox.y()) ||
          !std::isfinite(viewBox.width()) || !std::isfinite(viewBox.height()) ||
          viewBox.width() < kSvgMinDimension || viewBox.height() < kSvgMinDimension ||
          viewBox.width() > kSvgMaxDimension || viewBox.height() > kSvgMaxDimension ||
          info.intrinsicSize.width() > kSvgMaxDimension ||
          info.intrinsicSize.height() > kSvgMaxDimension) {
        error = QStringLiteral("SVG dimensions must be between 0.000001 and 16384 per axis");
      } else if (info.intrinsicSize.isEmpty()) {
        info.intrinsicSize = viewBox.size();
      }
    }
  }

  // The cache never refers to caller storage, including borrowed QByteArrays.
  // Oversized input leaves ownedSource empty and is not retained as a failure.
  state_->source = std::move(ownedSource);
  if (error) {
    state_->document = RejectedDocument{*error};
    updateTextureSize();
    update();
    Q_EMIT documentFailed(*error);
    return false;
  }
  state_->document = ReadyDocument{std::move(renderer), info};
  updateTextureSize();
  update();
  Q_EMIT documentLoaded(info.intrinsicSize, info.viewBox);
  return true;
}

void QtSvgItem::clearDocument() {
  requireOwner();
  if (std::holds_alternative<std::monostate>(state_->document)) {
    return;
  }
  state_->source.clear();
  state_->document = std::monostate{};
  updateTextureSize();
  update();
  Q_EMIT documentCleared();
}

void QtSvgItem::setResizeMode(SvgResizeMode mode) {
  requireOwner();
  if (mode != SvgResizeMode::Contain && mode != SvgResizeMode::Cover &&
      mode != SvgResizeMode::Stretch) {
    throw std::invalid_argument("SVG resize mode must be Contain, Cover or Stretch");
  }
  if (resizeMode_ != mode) {
    resizeMode_ = mode;
    update();
  }
}

SvgResizeMode QtSvgItem::resizeMode() const {
  requireOwner();
  return resizeMode_;
}

SvgDocumentState QtSvgItem::documentState() const {
  requireOwner();
  if (std::holds_alternative<ReadyDocument>(state_->document)) {
    return SvgDocumentState::Ready;
  }
  return std::holds_alternative<RejectedDocument>(state_->document)
      ? SvgDocumentState::Error : SvgDocumentState::Empty;
}

std::optional<SvgDocumentInfo> QtSvgItem::documentInfo() const {
  requireOwner();
  if (const auto* ready = std::get_if<ReadyDocument>(&state_->document)) {
    return ready->info;
  }
  return std::nullopt;
}

QString QtSvgItem::errorString() const {
  requireOwner();
  if (const auto* rejected = std::get_if<RejectedDocument>(&state_->document)) {
    return rejected->message;
  }
  return {};
}

void QtSvgItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
  if (state_) {
    updateTextureSize();
    update();
  }
}

void QtSvgItem::itemChange(ItemChange change, const ItemChangeData& data) {
  QQuickPaintedItem::itemChange(change, data);
  if (change == ItemDevicePixelRatioHasChanged && state_) {
    updateTextureSize();
  }
}

void QtSvgItem::updateTextureSize() {
  const qreal w = width();
  const qreal h = height();
  if (affinityLost_ || !std::holds_alternative<ReadyDocument>(state_->document) ||
      !std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0) {
    setTextureSize(QSize(1, 1));
    return;
  }
  const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
  const qreal safeDpr = std::isfinite(dpr) && dpr > 0 ? dpr : 1.0;
  // Qt multiplies textureSize by DPR. Bound both physical axes, preserving the
  // raster's aspect ratio. Rendering larger items remains a deliberate upscale.
  const qreal logicalLimit = std::max<qreal>(1.0, std::floor(kSvgMaxTexturePixels / safeDpr));
  const qreal scale = std::min<qreal>(1.0, logicalLimit / std::max(w, h));
  setTextureSize(QSize(std::clamp(int(std::ceil(w * scale)), 1, int(logicalLimit)),
                       std::clamp(int(std::ceil(h * scale)), 1, int(logicalLimit))));
}

void QtSvgItem::paint(QPainter* painter) {
  const auto* ready = std::get_if<ReadyDocument>(&state_->document);
  const QRectF bounds(0, 0, width(), height());
  if (affinityLost_ || !ready || !std::isfinite(bounds.width()) || !std::isfinite(bounds.height()) ||
      bounds.width() <= 0 || bounds.height() <= 0) {
    return;
  }
  QRectF destination = bounds;
  if (resizeMode_ != SvgResizeMode::Stretch) {
    const QSizeF source = ready->info.viewBox.size();
    const qreal sx = bounds.width() / source.width();
    const qreal sy = bounds.height() / source.height();
    const qreal scale = resizeMode_ == SvgResizeMode::Contain ? std::min(sx, sy) : std::max(sx, sy);
    const QSizeF size = source * scale;
    destination = QRectF(QPointF((bounds.width() - size.width()) / 2,
                                 (bounds.height() - size.height()) / 2), size);
  }
  if (!std::isfinite(destination.x()) || !std::isfinite(destination.y()) ||
      !std::isfinite(destination.width()) || !std::isfinite(destination.height())) return;
  // QQuickPaintedItem invokes paint while the GUI owner is blocked. Never
  // create QObjects, change source state, or dispatch application events here.
  painter->save();
  painter->setClipRect(bounds, Qt::IntersectClip);
  ready->renderer->render(painter, destination);
  painter->restore();
}

}  // namespace rnqnx::views
