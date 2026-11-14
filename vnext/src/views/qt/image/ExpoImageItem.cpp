#include "react-native-qnx/views/image/ExpoImageItem.h"

#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/image/BlurHash.h"
#include "react-native-qnx/views/image/ImageDiskCache.h"
#include "react-native-qnx/views/image/ImageEffects.h"
#include "react-native-qnx/views/image/ImageRefRegistry.h"
#include "react-native-qnx/views/image/ThumbHash.h"

#include <QBuffer>
#include <QLoggingCategory>
#include <QMovie>
#include <QPainter>
#include <QPromise>
#include <QQuickWindow>
#include <QThreadPool>
#include <QTransform>
#include <QtQml/qqml.h>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcExpoImage, "rnqnx.expoimage")

namespace rnqnx::views {
namespace {
// Blur radius is capped (in device pixels) so a large prop value cannot stall
// the GUI thread; the paint layer is capped so a huge view cannot exhaust memory.
constexpr qreal kMaxBlurRadius = 64;
constexpr int kMaxLayerPixels = 8192;

QVariantMap asMap(const QVariant& value) { return value.toMap(); }

ImageHeaders headersOf(const QVariantMap& entry) {
  ImageHeaders headers;
  const QVariantMap raw = entry.value(QStringLiteral("headers")).toMap();
  for (auto it = raw.cbegin(); it != raw.cend(); ++it) {
    headers.emplace(it.key().toUtf8(), it.value().toString().toUtf8());
  }
  return headers;
}

QEasingCurve::Type easingFor(const QString& timing) {
  if (timing == QLatin1String("ease-in")) return QEasingCurve::InQuad;
  if (timing == QLatin1String("ease-out")) return QEasingCurve::OutQuad;
  if (timing == QLatin1String("linear")) return QEasingCurve::Linear;
  return QEasingCurve::InOutQuad;
}

QImage hashImage(const QVariantMap& entry) {
  const QString blur = entry.value(QStringLiteral("blurhash")).toString();
  if (!blur.isEmpty()) {
    const int width = std::clamp(entry.value(QStringLiteral("width")).toInt(), 0, 128);
    const int height = std::clamp(entry.value(QStringLiteral("height")).toInt(), 0, 128);
    return decodeBlurHash(blur, width > 0 ? width : 32, height > 0 ? height : 32);
  }
  return decodeThumbHash(entry.value(QStringLiteral("thumbhash")).toString());
}

}  // namespace

struct ExpoImageItem::Layer {
  QImage image;
  QSize original;
  bool animated{false};
  bool placeholder{false};
  std::unique_ptr<QBuffer> buffer;
  std::unique_ptr<QMovie> movie;
  mutable QImage cache;
  mutable QString cacheKey;

  QImage frame() const { return movie ? movie->currentImage() : image; }
};

struct ExpoImageItem::Choice {
  enum class Kind { None, Uri, Hash, Ref } kind{Kind::None};
  QString uri;
  quint32 refId{0};
  ImageHeaders headers;
  QString cacheKey;
  QVariantMap entry;
};

ExpoImageItem::ExpoImageItem(QQuickItem* parent)
    : QQuickPaintedItem(parent),
      fetcher_(30000, &ImageDiskCache::shared(), &ImageFetchQueue::shared()) {
  setRenderTarget(QQuickPaintedItem::Image);
  setAntialiasing(true);
  setFlag(ItemHasContents, true);
  setAcceptedMouseButtons(Qt::NoButton);
  transitionAnimation_.setStartValue(0.0);
  transitionAnimation_.setEndValue(1.0);
  QObject::connect(&transitionAnimation_, &QVariantAnimation::valueChanged, this,
                   [this](const QVariant& value) {
                     transitionProgress_ = value.toReal();
                     update();
                   });
  QObject::connect(&transitionAnimation_, &QVariantAnimation::finished, this,
                   [this] { finishTransition(); });
  QObject::connect(&decodeWatcher_, &QFutureWatcher<DecodedImage>::finished, this,
                   [this] { onDecoded(); });
  QObject::connect(&placeholderWatcher_, &QFutureWatcher<DecodedImage>::finished, this, [this] {
    if (placeholderGeneration_ != generation_ || current_) return;
    DecodedImage decoded = placeholderWatcher_.result();
    if (!decoded.error.isEmpty()) return;
    auto layer = std::make_unique<Layer>();
    layer->image = std::move(decoded.first);
    layer->original = decoded.originalSize;
    layer->placeholder = true;
    placeholderLayer_ = std::move(layer);
    update();
  });
}

ExpoImageItem::~ExpoImageItem() {
  fetcher_.cancel(mainFetch_);
  fetcher_.cancel(placeholderFetch_);
}

void ExpoImageItem::registerQmlType() {
  static const int id =
      qmlRegisterType<ExpoImageItem>("Rnqnx.ExpoImage", 1, 0, "ExpoImageItem");
  (void)id;
}

// ---- props ----------------------------------------------------------------

void ExpoImageItem::setSource(const QVariantList& value) {
  if (value == source_) return;
  source_ = value;
  scheduleLoad();
}

void ExpoImageItem::setPlaceholder(const QVariantList& value) {
  if (value == placeholder_) return;
  placeholder_ = value;
  if (!current_) scheduleLoad();
}

void ExpoImageItem::setContentFit(const QString& value) {
  if (value == contentFitText_) return;
  contentFitText_ = value;
  fit_ = parseContentFit(value);
  invalidate();
  ensureResolution();
}

void ExpoImageItem::setAllowDownscaling(bool value) {
  if (value == allowDownscaling_) return;
  allowDownscaling_ = value;
  ensureResolution();
}

void ExpoImageItem::setPlaceholderContentFit(const QString& value) {
  placeholderFitText_ = value;
  placeholderFit_ = value.isEmpty() ? ContentFit::ScaleDown : parseContentFit(value);
  invalidate();
}

void ExpoImageItem::setContentPosition(const QVariant& value) {
  if (value == positionRaw_) return;
  positionRaw_ = value;
  position_ = parseContentPosition(value);
  invalidate();
}

void ExpoImageItem::setTransition(const QVariantMap& value) {
  if (value == transitionRaw_) return;
  transitionRaw_ = value;
  transitionMs_ = std::max(0, value.value(QStringLiteral("duration")).toInt());
  const QVariant effect = value.value(QStringLiteral("effect"));
  effect_ = effect.isValid() && !effect.isNull() ? effect.toString() : QStringLiteral("cross-dissolve");
  if (effect_ == QLatin1String("curl-up") || effect_ == QLatin1String("curl-down")) {
    // expo-image documents the curl effects as iOS-only (Image.types.ts: "On
    // Android, only 'cross-dissolve' is supported"); a QPainter transform
    // cannot bend the picture, so they behave as they do on Android.
    qCWarning(lcExpoImage, "transition effect '%s' is iOS-only; using cross-dissolve",
              qPrintable(effect_));
    effect_ = QStringLiteral("cross-dissolve");
  }
  easing_ = easingFor(value.value(QStringLiteral("timing")).toString());
}

void ExpoImageItem::setTintColor(const QColor& value) {
  if (value == tint_) return;
  tint_ = value;
  invalidate();
}
void ExpoImageItem::setHasTint(bool value) {
  if (value == hasTint_) return;
  hasTint_ = value;
  invalidate();
}
void ExpoImageItem::setBlurRadius(qreal value) {
  const qreal radius = std::clamp<qreal>(value, 0, kMaxBlurRadius);
  if (radius == blurRadius_) return;
  blurRadius_ = radius;
  invalidate();
}

void ExpoImageItem::setCachePolicy(const QString& value) {
  cachePolicyText_ = value;
  policy_ = parseCachePolicy(value);
}

void ExpoImageItem::setRecyclingKey(const QString& value) {
  if (value == recyclingKey_) return;
  const bool hadKey = !recyclingKey_.isEmpty();
  recyclingKey_ = value;
  // A recycled view must not show the previous owner's picture while the new
  // one loads, so the old content goes at once, without a transition.
  if (hadKey || !value.isEmpty()) {
    current_.reset();
    outgoing_.reset();
    placeholderLayer_.reset();
    transitionAnimation_.stop();
    transitionProgress_ = 1.0;
    setDisplayed(false);
    update();
    // Bumps the generation and cancels a fetch for the previous owner, then
    // loads the source for the new one (setSource ignores an unchanged source).
    scheduleLoad();
  }
}

void ExpoImageItem::setAutoplay(bool value) {
  autoplay_ = value;
  if (current_ && current_->movie) value ? startAnimating() : stopAnimating();
}

void ExpoImageItem::setBackgroundColor(const QColor& value) {
  background_ = value;
  update();
}
void ExpoImageItem::setRadius(qreal value) {
  radius_ = value;
  update();
}
void ExpoImageItem::setCorner(int index, qreal value) {
  corners_[static_cast<size_t>(index)] = value;
  update();
}
void ExpoImageItem::setUsePerCornerRadius(bool value) {
  perCorner_ = value;
  update();
}
void ExpoImageItem::setBorderWidth(qreal value) {
  borderWidth_ = value;
  update();
}
void ExpoImageItem::setBorderColor(const QColor& value) {
  borderColor_ = value;
  update();
}

void ExpoImageItem::setTestId(const QString& value) {
  testId_ = value;
  setObjectName(value);
}

void ExpoImageItem::setFocusable(bool value) {
  focusable_ = value;
  setActiveFocusOnTab(value);
}

CornerRadii ExpoImageItem::radii() const {
  return perCorner_ ? corners_ : CornerRadii{radius_, radius_, radius_, radius_};
}

bool ExpoImageItem::contains(const QPointF& point) const {
  // `none` and `box-none` never take a touch: this view has no children.
  if (pointerEvents_ == QLatin1String("none") || pointerEvents_ == QLatin1String("box-none")) {
    return false;
  }
  const QRectF area = boundingRect().adjusted(-hitSlop_.left(), -hitSlop_.top(), hitSlop_.right(),
                                              hitSlop_.bottom());
  return area.contains(point);
}

bool ExpoImageItem::animating() const {
  return current_ && current_->movie && current_->movie->state() == QMovie::Running;
}

// ---- loading ----------------------------------------------------------------

void ExpoImageItem::invalidate() {
  if (current_) current_->cacheKey.clear();
  if (placeholderLayer_) placeholderLayer_->cacheKey.clear();
  update();
}

void ExpoImageItem::geometryChange(const QRectF& next, const QRectF& previous) {
  QQuickPaintedItem::geometryChange(next, previous);
  if (next.size() != previous.size()) {
    invalidate();
    ensureResolution();
  }
}

void ExpoImageItem::itemChange(ItemChange change, const ItemChangeData& data) {
  QQuickPaintedItem::itemChange(change, data);
  if (change == ItemDevicePixelRatioHasChanged) {
    invalidate();
    ensureResolution();
  }
}

void ExpoImageItem::ensureResolution() {
  if (!current_ || current_->placeholder || current_->animated) return;
  const Choice choice = choose(source_);
  if (choice.kind != Choice::Kind::Uri) return;
  const QSize decoded = current_->image.size();
  const QSize original = current_->original;
  const QSize target = targetPixels();
  const bool reduced = decoded.width() < original.width() || decoded.height() < original.height();
  const bool needsOriginal = !allowDownscaling_ || fit_ == ContentFit::None || fit_ == ContentFit::Fill;
  const bool grew = target.width() > decoded.width() || target.height() > decoded.height();
  if (choice.uri != shownUri_ || (reduced && (needsOriginal || grew))) {
    // Coalesce layout changes and respect lockResource. Keep the current layer
    // visible while the selected source is fetched/decoded at its new size.
    scheduleLoad();
  }
}

void ExpoImageItem::scheduleLoad(bool ignoreLock) {
  if (locked_ && !ignoreLock) {
    loadDeferred_ = true;
    return;
  }
  if (loadPending_) return;
  loadPending_ = true;
  // Props and layout arrive in one mount transaction; loading after it lets
  // the decoder see the final size.
  QMetaObject::invokeMethod(this, [this] { startLoad(); }, Qt::QueuedConnection);
}

void ExpoImageItem::reload() {
  bypassMemory_ = true;
  loadDeferred_ = false;
  scheduleLoad(/*ignoreLock=*/true);
}

void ExpoImageItem::lockResource() { locked_ = true; }

void ExpoImageItem::unlockResource() {
  locked_ = false;
  if (loadDeferred_) {
    loadDeferred_ = false;
    scheduleLoad();
  }
}

QSize ExpoImageItem::targetPixels() const {
  const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
  const int w = static_cast<int>(std::ceil(width() * dpr));
  const int h = static_cast<int>(std::ceil(height() * dpr));
  // A view with no size yet (a hidden tab, before layout) has no target: the
  // decoder keeps the full size instead of scaling to nothing.
  return w > 0 && h > 0 ? QSize(w, h) : QSize();
}

ExpoImageItem::Choice ExpoImageItem::choose(const QVariantList& entries) const {
  Choice choice;
  const int target = targetPixels().width();
  int bestWidth = -1;
  bool bestFits = false;
  for (const QVariant& raw : entries) {
    const QVariantMap entry = asMap(raw);
    if (entry.contains(QStringLiteral("imageRef"))) {
      // An ImageRef from Image.loadAsync: the picture is already decoded.
      choice.kind = Choice::Kind::Ref;
      choice.refId = entry.value(QStringLiteral("imageRef")).toUInt();
      return choice;
    }
    const QString uri = entry.value(QStringLiteral("uri")).toString();
    if (uri.isEmpty()) {
      if (choice.kind == Choice::Kind::None &&
          (entry.contains(QStringLiteral("blurhash")) || entry.contains(QStringLiteral("thumbhash")))) {
        choice.kind = Choice::Kind::Hash;
        choice.entry = entry;
      }
      continue;
    }
    // Several resolutions: the smallest that covers the view, else the largest.
    const int width = entry.value(QStringLiteral("width")).toInt();
    const bool fits = width > 0 && width >= target;
    const bool better = choice.kind != Choice::Kind::Uri || (fits && (!bestFits || width < bestWidth)) ||
                        (!bestFits && !fits && width > bestWidth);
    if (better) {
      choice.kind = Choice::Kind::Uri;
      choice.uri = uri;
      choice.headers = headersOf(entry);
      choice.cacheKey = entry.value(QStringLiteral("cacheKey")).toString();
      bestWidth = width;
      bestFits = fits;
    }
  }
  return choice;
}

void ExpoImageItem::startLoad() {
  loadPending_ = false;
  const bool bypass = bypassMemory_;
  bypassMemory_ = false;
  fetcher_.cancel(mainFetch_);
  mainFetch_ = 0;
  const quint64 generation = ++generation_;
  const Choice choice = choose(source_);
  shownUri_ = choice.uri;
  Q_EMIT sourceChanged();
  setDisplayed(false);

  if (choice.kind == Choice::Kind::None) {
    current_.reset();
    outgoing_.reset();
    loadPlaceholder();
    update();
    return;
  }
  if (choice.kind == Choice::Kind::Hash) {
    const QImage image = hashImage(choice.entry);
    if (image.isNull()) {
      Q_EMIT failed(QStringLiteral("Invalid blurhash or thumbhash"));
      return;
    }
    auto layer = std::make_unique<Layer>();
    layer->image = image;
    layer->original = image.size();
    show(std::move(layer), QStringLiteral("none"), QString());
    return;
  }

  if (choice.kind == Choice::Kind::Ref) {
    const ImageRefEntry entry = ImageRefRegistry::shared().find(choice.refId);
    if (entry.image.isNull()) {
      Q_EMIT failed(QStringLiteral("The ImageRef was released"));
      return;
    }
    auto layer = std::make_unique<Layer>();
    layer->image = entry.image;
    layer->original = entry.image.size();
    mediaType_ = entry.mediaType;
    show(std::move(layer), QStringLiteral("memory"), QString());
    return;
  }

  ImageRequest request;
  request.uri = choice.uri;
  request.headers = choice.headers;
  request.cacheKey = choice.cacheKey;
  request.policy = policy_;
  request.priority = parseImagePriority(priority_);
  const QString key = request.storageKey();

  if (!bypass && usesMemory(policy_)) {
    const QImage cached = DecodedImageCache::shared().image(DecodedImageCache::key(key));
    if (!cached.isNull()) {
      auto layer = std::make_unique<Layer>();
      layer->image = cached;
      layer->original = cached.size();
      show(std::move(layer), QStringLiteral("memory"), choice.uri);
      return;
    }
  }

  Q_EMIT loadStarted();
  loadPlaceholder();
  mainFetch_ = fetcher_.fetch(
      request,
      [this, generation](qint64 received, qint64 total) {
        if (generation == generation_) Q_EMIT progressed(static_cast<double>(received), static_cast<double>(total));
      },
      [this, generation, key, uri = choice.uri, policy = policy_](FetchResult result) {
        if (generation != generation_) return;
        if (!result.error.isEmpty()) {
          Q_EMIT failed(result.error);
          return;
        }
        pendingCacheType_ = result.origin == ImageOrigin::Disk ? QStringLiteral("disk")
                                                              : QStringLiteral("none");
        pendingUrl_ = uri;
        pendingKey_ = key;
        pendingStore_ = usesMemory(policy) && result.cacheable;
        decodeGeneration_ = generation;
        const QSize target = targetPixels();
        const bool allow = allowDownscaling_;
        const ContentFit fit = fit_;
        auto promise = std::make_shared<QPromise<DecodedImage>>();
        promise->start();
        decodeWatcher_.setFuture(promise->future());
        QThreadPool::globalInstance()->start(
            [promise, bytes = std::move(result.bytes), target, allow, fit] {
              promise->addResult(decodeImage(bytes, target, allow, fit));
              promise->finish();
            });
      });
}

void ExpoImageItem::loadPlaceholder() {
  fetcher_.cancel(placeholderFetch_);
  placeholderFetch_ = 0;
  if (current_) return;
  const Choice choice = choose(placeholder_);
  if (choice.kind == Choice::Kind::None) {
    placeholderLayer_.reset();
    return;
  }
  if (choice.kind == Choice::Kind::Hash || choice.kind == Choice::Kind::Ref) {
    const QImage image = choice.kind == Choice::Kind::Hash
                             ? hashImage(choice.entry)
                             : ImageRefRegistry::shared().find(choice.refId).image;
    if (image.isNull()) return;
    auto layer = std::make_unique<Layer>();
    layer->image = image;
    layer->original = image.size();
    layer->placeholder = true;
    placeholderLayer_ = std::move(layer);
    update();
    return;
  }
  ImageRequest request;
  request.uri = choice.uri;
  request.headers = choice.headers;
  request.cacheKey = choice.cacheKey;
  request.policy = ImageCachePolicy::MemoryDisk;
  const QImage cached = DecodedImageCache::shared().image(DecodedImageCache::key(request.storageKey()));
  if (!cached.isNull()) {
    auto layer = std::make_unique<Layer>();
    layer->image = cached;
    layer->original = cached.size();
    layer->placeholder = true;
    placeholderLayer_ = std::move(layer);
    update();
    return;
  }
  const quint64 generation = generation_;
  placeholderFetch_ = fetcher_.fetch(request, {}, [this, generation](FetchResult result) {
    if (generation != generation_ || !result.error.isEmpty() || current_) return;
    placeholderGeneration_ = generation;
    auto promise = std::make_shared<QPromise<DecodedImage>>();
    promise->start();
    placeholderWatcher_.setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise, bytes = std::move(result.bytes)] {
      promise->addResult(decodeImage(bytes, QSize(), false, ContentFit::Cover));
      promise->finish();
    });
  });
}

void ExpoImageItem::onDecoded() {
  if (decodeGeneration_ != generation_) return;
  DecodedImage decoded = decodeWatcher_.result();
  if (!decoded.error.isEmpty()) {
    Q_EMIT failed(QStringLiteral("Could not load ") + pendingUrl_);
    return;
  }
  const QString mediaType = decoded.mediaType;
  const bool animated = decoded.animated;
  if (pendingStore_ && !decoded.animated && !decoded.downscaled) {
    DecodedImageCache::shared().insert(pendingKey_, decoded.first);
  }
  auto layer = std::make_unique<Layer>();
  layer->image = std::move(decoded.first);
  layer->original = decoded.originalSize;
  layer->animated = animated;
  if (animated) adoptMovie(*layer, decoded.bytes);
  mediaType_ = mediaType;
  show(std::move(layer), pendingCacheType_, pendingUrl_);
}

void ExpoImageItem::adoptMovie(Layer& layer, const QByteArray& bytes) {
  layer.buffer = std::make_unique<QBuffer>();
  layer.buffer->setData(bytes);
  layer.buffer->open(QIODevice::ReadOnly);
  layer.movie = std::make_unique<QMovie>(layer.buffer.get());
  layer.movie->setCacheMode(QMovie::CacheNone);
  if (!layer.movie->isValid()) {
    layer.movie.reset();
    layer.buffer.reset();
    layer.animated = false;
    return;
  }
  QObject::connect(layer.movie.get(), &QMovie::frameChanged, this, [this] {
    if (current_) current_->cacheKey.clear();
    update();
  });
  QObject::connect(layer.movie.get(), &QMovie::stateChanged, this,
                   [this] { Q_EMIT animatingChanged(); });
}

void ExpoImageItem::show(std::unique_ptr<Layer> layer, const QString& cacheType, const QString& url) {
  std::unique_ptr<Layer> previous = std::move(current_);
  if (!previous) previous = std::move(placeholderLayer_);
  placeholderLayer_.reset();
  current_ = std::move(layer);
  const QSize size = current_->original;
  const bool animated = current_->movie != nullptr;
  if (animated && autoplay_) current_->movie->start();
  Q_EMIT loaded(url, size.width(), size.height(), cacheType, mediaType_, animated);
  beginTransition(std::move(previous));
  // Geometry/options may have changed while the worker decoded its snapshot,
  // including before the first layer existed. Recheck against the live props.
  ensureResolution();
}

void ExpoImageItem::beginTransition(std::unique_ptr<Layer> previous) {
  transitionAnimation_.stop();
  if (transitionMs_ <= 0) {
    outgoing_.reset();
    transitionProgress_ = 1.0;
    update();
    setDisplayed(true);
    return;
  }
  outgoing_ = std::move(previous);
  transitionProgress_ = 0.0;
  transitionAnimation_.setDuration(transitionMs_);
  transitionAnimation_.setEasingCurve(QEasingCurve(easing_));
  transitionAnimation_.start();
  update();
}

void ExpoImageItem::finishTransition() {
  outgoing_.reset();
  transitionProgress_ = 1.0;
  update();
  setDisplayed(true);
}

void ExpoImageItem::setDisplayed(bool value) {
  if (displayed_ == value) return;
  displayed_ = value;
  Q_EMIT displayedChanged();
}

void ExpoImageItem::startAnimating() {
  if (current_ && current_->movie) current_->movie->start();
}
void ExpoImageItem::stopAnimating() {
  if (current_ && current_->movie) current_->movie->setPaused(true);
}

// ---- painting ---------------------------------------------------------------

QImage ExpoImageItem::rendered(Layer& layer) const {
  const ContentFit fit = layer.placeholder ? placeholderFit_ : fit_;
  const bool applyEffects = !layer.placeholder;
  const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
  const QImage frame = layer.frame();
  const QString key = QStringLiteral("%1x%2@%3|%4|%5|%6|%7|%8|%9")
                          .arg(width()).arg(height()).arg(dpr).arg(static_cast<int>(fit))
                          .arg(applyEffects ? blurRadius_ : 0)
                          .arg(applyEffects && hasTint_ ? tint_.name(QColor::HexArgb) : QString())
                          .arg(frame.cacheKey())
                          .arg(positionRaw_.toString())
                          .arg(layer.movie ? layer.movie->currentFrameNumber() : -1);
  if (!layer.cache.isNull() && layer.cacheKey == key) return layer.cache;

  const QSize pixels(std::clamp(static_cast<int>(std::ceil(width() * dpr)), 1, kMaxLayerPixels),
                     std::clamp(static_cast<int>(std::ceil(height() * dpr)), 1, kMaxLayerPixels));
  QImage out(pixels, QImage::Format_ARGB32_Premultiplied);
  if (out.isNull()) return layer.cache;
  out.fill(Qt::transparent);
  const QRectF target = fittedRect(QSizeF(layer.original), QSizeF(width(), height()), fit, position_, 1.0);
  {
    QPainter painter(&out);
    painter.setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);
    painter.scale(dpr, dpr);
    painter.drawImage(target, frame);
  }
  if (applyEffects && blurRadius_ > 0) out = blurImage(out, static_cast<int>(std::lround(std::min<qreal>(blurRadius_ * dpr, kMaxBlurRadius))));
  if (applyEffects && hasTint_) out = tintImage(out, tint_);
  layer.cache = out;
  layer.cacheKey = key;
  return out;
}

void ExpoImageItem::drawLayer(QPainter& painter, Layer& layer, qreal opacity,
                              const QTransform& transform) {
  const QImage image = rendered(layer);
  painter.save();
  painter.setOpacity(painter.opacity() * opacity);
  painter.setTransform(transform, true);
  painter.drawImage(QRectF(0, 0, width(), height()), image, QRectF(0, 0, image.width(), image.height()));
  painter.restore();
}

void ExpoImageItem::paint(QPainter* painter) {
  painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
  const QRectF bounds = boundingRect();
  const QPainterPath path = roundedRectPath(bounds, radii());
  painter->save();
  painter->setClipPath(path);
  if (background_.alpha() > 0) painter->fillPath(path, background_);

  const bool transitioning = outgoing_ && transitionProgress_ < 1.0;
  if (!current_ && placeholderLayer_) {
    drawLayer(*painter, *placeholderLayer_, 1.0, QTransform());
  } else if (current_ && !transitioning) {
    drawLayer(*painter, *current_, 1.0, QTransform());
  } else if (current_) {
    const qreal t = transitionProgress_;
    if (!effect_.startsWith(QLatin1String("flip-from-"))) {
      drawLayer(*painter, *outgoing_, 1.0, QTransform());
      drawLayer(*painter, *current_, t, QTransform());
    } else {
      const bool vertical = effect_ == QLatin1String("flip-from-top") ||
                            effect_ == QLatin1String("flip-from-bottom");
      const qreal direction =
          (effect_ == QLatin1String("flip-from-top") || effect_ == QLatin1String("flip-from-left")) ? 1 : -1;
      // The old picture turns away over the first half, the new one turns in
      // over the second; at the midpoint the picture is edge-on.
      const auto about = [&](qreal angle) {
        QTransform transform;
        transform.translate(width() / 2, height() / 2);
        transform.rotate(angle, vertical ? Qt::XAxis : Qt::YAxis);
        transform.translate(-width() / 2, -height() / 2);
        return transform;
      };
      if (t < 0.5) drawLayer(*painter, *outgoing_, 1.0, about(direction * t * 180.0));
      else drawLayer(*painter, *current_, 1.0, about(-direction * (1.0 - t) * 180.0));
    }
  }
  painter->restore();

  if (borderWidth_ > 0 && borderColor_.alpha() > 0) {
    const qreal half = borderWidth_ / 2;
    CornerRadii inner = radii();
    for (qreal& r : inner) r = std::max<qreal>(0, r - half);
    painter->setPen(QPen(borderColor_, borderWidth_));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(roundedRectPath(bounds.adjusted(half, half, -half, -half), inner));
  }
}

QImage ExpoImageItem::grabImage() {
  const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
  QImage image(std::max(1, static_cast<int>(std::ceil(width() * dpr))),
               std::max(1, static_cast<int>(std::ceil(height() * dpr))),
               QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.scale(dpr, dpr);
  paint(&painter);
  return image;
}

}  // namespace rnqnx::views
