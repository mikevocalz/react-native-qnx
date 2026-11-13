#pragma once

#include "react-native-qnx/views/image/ImageDecode.h"
#include "react-native-qnx/views/image/ImageLayout.h"
#include "react-native-qnx/views/image/QtImageFetcher.h"

#include <QColor>
#include <QFutureWatcher>
#include <QImage>
#include <QMarginsF>
#include <QQuickPaintedItem>
#include <QVariant>
#include <QVariantAnimation>

#include <memory>

class QMovie;
class QBuffer;

namespace rnqnx::views {

/// The view behind expo-image's <Image>. It loads, decodes and paints the
/// picture itself with QPainter instead of stacking QML Image items, so
/// rounded corners, tint and blur need no shader module (the Pi's Qt runtime
/// ships none) and every corner, fit and transition renders the same on every
/// backend. Sources come from the shared decoded cache, the disk cache, the
/// network (with the source's headers) or a file.
class ExpoImageItem : public QQuickPaintedItem {
  Q_OBJECT
  // Image props
  Q_PROPERTY(QVariantList source READ source WRITE setSource NOTIFY sourceChanged)
  Q_PROPERTY(QVariantList placeholder READ placeholder WRITE setPlaceholder)
  Q_PROPERTY(QString contentFit READ contentFit WRITE setContentFit)
  Q_PROPERTY(QString placeholderContentFit READ placeholderContentFit WRITE setPlaceholderContentFit)
  Q_PROPERTY(QVariant contentPosition READ contentPosition WRITE setContentPosition)
  Q_PROPERTY(QVariantMap transition READ transition WRITE setTransition)
  Q_PROPERTY(QColor tintColor READ tintColor WRITE setTintColor)
  Q_PROPERTY(bool hasTint READ hasTint WRITE setHasTint)
  Q_PROPERTY(qreal blurRadius READ blurRadius WRITE setBlurRadius)
  Q_PROPERTY(QString cachePolicy READ cachePolicy WRITE setCachePolicy)
  Q_PROPERTY(QString priority READ priority WRITE setPriority)
  Q_PROPERTY(QString recyclingKey READ recyclingKey WRITE setRecyclingKey)
  Q_PROPERTY(bool allowDownscaling READ allowDownscaling WRITE setAllowDownscaling)
  Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay)
  // Style props the mounting layer sets on every item
  Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
  Q_PROPERTY(qreal radius READ radius WRITE setRadius)
  Q_PROPERTY(qreal topLeftRadius READ topLeftRadius WRITE setTopLeftRadius)
  Q_PROPERTY(qreal topRightRadius READ topRightRadius WRITE setTopRightRadius)
  Q_PROPERTY(qreal bottomRightRadius READ bottomRightRadius WRITE setBottomRightRadius)
  Q_PROPERTY(qreal bottomLeftRadius READ bottomLeftRadius WRITE setBottomLeftRadius)
  Q_PROPERTY(bool usePerCornerRadius READ usePerCornerRadius WRITE setUsePerCornerRadius)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
  // Common view props
  Q_PROPERTY(QString testId READ testId WRITE setTestId)
  Q_PROPERTY(QString nativeId READ nativeId WRITE setNativeId)
  Q_PROPERTY(QString pointerEvents READ pointerEvents WRITE setPointerEvents)
  Q_PROPERTY(QMarginsF hitSlop READ hitSlop WRITE setHitSlop)
  Q_PROPERTY(bool focusable READ focusable WRITE setFocusable)
  // Accessibility (read by the QML wrapper's Accessible properties)
  Q_PROPERTY(bool accessible READ accessible WRITE setAccessible NOTIFY accessibilityChanged)
  Q_PROPERTY(QString accessibilityLabel READ accessibilityLabel WRITE setAccessibilityLabel NOTIFY accessibilityChanged)
  Q_PROPERTY(QString accessibilityRole READ accessibilityRole WRITE setAccessibilityRole NOTIFY accessibilityChanged)
  Q_PROPERTY(QString accessibilityHint READ accessibilityHint WRITE setAccessibilityHint NOTIFY accessibilityChanged)
  Q_PROPERTY(QString accessibilityValue READ accessibilityValue WRITE setAccessibilityValue NOTIFY accessibilityChanged)
  Q_PROPERTY(bool accessibilityStateDisabled READ accessibilityStateDisabled WRITE setAccessibilityStateDisabled NOTIFY accessibilityChanged)
  Q_PROPERTY(bool accessibilityStateSelected READ accessibilityStateSelected WRITE setAccessibilityStateSelected NOTIFY accessibilityChanged)
  Q_PROPERTY(bool accessibilityStateChecked READ accessibilityStateChecked WRITE setAccessibilityStateChecked NOTIFY accessibilityChanged)
  // Read-only state
  Q_PROPERTY(bool displayed READ displayed NOTIFY displayedChanged)
  Q_PROPERTY(bool animating READ animating NOTIFY animatingChanged)
  Q_PROPERTY(QString shownUri READ shownUri NOTIFY sourceChanged)

 public:
  explicit ExpoImageItem(QQuickItem* parent = nullptr);
  ~ExpoImageItem() override;

  /// Registers the QML type `Rnqnx.ExpoImage.ExpoImageItem`. Safe to call twice.
  static void registerQmlType();

  void paint(QPainter* painter) override;
  bool contains(const QPointF& point) const override;

  QVariantList source() const { return source_; }
  void setSource(const QVariantList& value);
  QVariantList placeholder() const { return placeholder_; }
  void setPlaceholder(const QVariantList& value);
  QString contentFit() const { return contentFitText_; }
  void setContentFit(const QString& value);
  QString placeholderContentFit() const { return placeholderFitText_; }
  void setPlaceholderContentFit(const QString& value);
  QVariant contentPosition() const { return positionRaw_; }
  void setContentPosition(const QVariant& value);
  QVariantMap transition() const { return transitionRaw_; }
  void setTransition(const QVariantMap& value);
  QColor tintColor() const { return tint_; }
  void setTintColor(const QColor& value);
  bool hasTint() const { return hasTint_; }
  void setHasTint(bool value);
  qreal blurRadius() const { return blurRadius_; }
  void setBlurRadius(qreal value);
  QString cachePolicy() const { return cachePolicyText_; }
  void setCachePolicy(const QString& value);
  QString priority() const { return priority_; }
  void setPriority(const QString& value) { priority_ = value; }
  QString recyclingKey() const { return recyclingKey_; }
  void setRecyclingKey(const QString& value);
  bool allowDownscaling() const { return allowDownscaling_; }
  void setAllowDownscaling(bool value) { allowDownscaling_ = value; }
  bool autoplay() const { return autoplay_; }
  void setAutoplay(bool value);

  QColor backgroundColor() const { return background_; }
  void setBackgroundColor(const QColor& value);
  qreal radius() const { return radius_; }
  void setRadius(qreal value);
  qreal topLeftRadius() const { return corners_[0]; }
  void setTopLeftRadius(qreal value) { setCorner(0, value); }
  qreal topRightRadius() const { return corners_[1]; }
  void setTopRightRadius(qreal value) { setCorner(1, value); }
  qreal bottomRightRadius() const { return corners_[2]; }
  void setBottomRightRadius(qreal value) { setCorner(2, value); }
  qreal bottomLeftRadius() const { return corners_[3]; }
  void setBottomLeftRadius(qreal value) { setCorner(3, value); }
  bool usePerCornerRadius() const { return perCorner_; }
  void setUsePerCornerRadius(bool value);
  qreal borderWidth() const { return borderWidth_; }
  void setBorderWidth(qreal value);
  QColor borderColor() const { return borderColor_; }
  void setBorderColor(const QColor& value);

  QString testId() const { return testId_; }
  void setTestId(const QString& value);
  QString nativeId() const { return nativeId_; }
  void setNativeId(const QString& value) { nativeId_ = value; }
  QString pointerEvents() const { return pointerEvents_; }
  void setPointerEvents(const QString& value) { pointerEvents_ = value; }
  QMarginsF hitSlop() const { return hitSlop_; }
  void setHitSlop(const QMarginsF& value) { hitSlop_ = value; }
  bool focusable() const { return focusable_; }
  void setFocusable(bool value);

  bool accessible() const { return accessible_; }
  void setAccessible(bool value) { assign(accessible_, value); }
  QString accessibilityLabel() const { return a11yLabel_; }
  void setAccessibilityLabel(const QString& value) { assign(a11yLabel_, value); }
  QString accessibilityRole() const { return a11yRole_; }
  void setAccessibilityRole(const QString& value) { assign(a11yRole_, value); }
  QString accessibilityHint() const { return a11yHint_; }
  void setAccessibilityHint(const QString& value) { assign(a11yHint_, value); }
  QString accessibilityValue() const { return a11yValue_; }
  void setAccessibilityValue(const QString& value) { assign(a11yValue_, value); }
  bool accessibilityStateDisabled() const { return a11yDisabled_; }
  void setAccessibilityStateDisabled(bool value) { assign(a11yDisabled_, value); }
  bool accessibilityStateSelected() const { return a11ySelected_; }
  void setAccessibilityStateSelected(bool value) { assign(a11ySelected_, value); }
  bool accessibilityStateChecked() const { return a11yChecked_; }
  void setAccessibilityStateChecked(bool value) { assign(a11yChecked_, value); }

  bool displayed() const { return displayed_; }
  bool animating() const;
  /// The URI of the source currently shown or loading (not the resolved URL).
  QString shownUri() const { return shownUri_; }

  /// Animated images (GIF, WebP): start or stop playback.
  Q_INVOKABLE void startAnimating();
  Q_INVOKABLE void stopAnimating();
  /// Loads the current source again, bypassing the memory cache.
  Q_INVOKABLE void reload();
  /// The painted result at the item's size, for tests and screenshots.
  Q_INVOKABLE QImage grabImage();

 Q_SIGNALS:
  void sourceChanged();
  void loadStarted();
  /// \a cacheType is `none`, `disk` or `memory`.
  void progressed(double loaded, double total);
  void loaded(const QString& url, double width, double height, const QString& cacheType,
              const QString& mediaType, bool isAnimated);
  void failed(const QString& message);
  void displayedChanged();
  void animatingChanged();
  void accessibilityChanged();

 protected:
  void geometryChange(const QRectF& next, const QRectF& previous) override;
  void itemChange(ItemChange change, const ItemChangeData& data) override;

 private:
  struct Layer;
  struct Choice;

  template <typename T>
  void assign(T& field, const T& value) {
    if (field == value) return;
    field = value;
    Q_EMIT accessibilityChanged();
  }
  void setCorner(int index, qreal value);
  void invalidate();
  void scheduleLoad();
  void startLoad();
  void loadPlaceholder();
  Choice choose(const QVariantList& entries) const;
  void show(std::unique_ptr<Layer> layer, const QString& cacheType, const QString& url);
  void onDecoded();
  void beginTransition(std::unique_ptr<Layer> previous);
  void finishTransition();
  void setDisplayed(bool value);
  CornerRadii radii() const;
  QImage rendered(Layer& layer) const;
  void drawLayer(QPainter& painter, Layer& layer, qreal opacity, const QTransform& transform);
  QSize targetPixels() const;
  void adoptMovie(Layer& layer, const QByteArray& bytes);

  QVariantList source_;
  QVariantList placeholder_;
  QString contentFitText_{QStringLiteral("cover")};
  QString placeholderFitText_{QStringLiteral("scale-down")};
  ContentFit fit_{ContentFit::Cover};
  ContentFit placeholderFit_{ContentFit::ScaleDown};
  QVariant positionRaw_;
  ContentPosition position_;
  QVariantMap transitionRaw_;
  int transitionMs_{0};
  QString effect_{QStringLiteral("cross-dissolve")};
  QEasingCurve::Type easing_{QEasingCurve::InOutQuad};
  QColor tint_{Qt::transparent};
  bool hasTint_{false};
  qreal blurRadius_{0};
  QString cachePolicyText_{QStringLiteral("memory-disk")};
  ImageCachePolicy policy_{ImageCachePolicy::MemoryDisk};
  QString priority_;
  QString recyclingKey_;
  bool allowDownscaling_{true};
  bool autoplay_{true};
  QColor background_{Qt::transparent};
  qreal radius_{0};
  CornerRadii corners_{0, 0, 0, 0};
  bool perCorner_{false};
  qreal borderWidth_{0};
  QColor borderColor_{Qt::transparent};
  QString testId_;
  QString nativeId_;
  QString pointerEvents_{QStringLiteral("auto")};
  QMarginsF hitSlop_;
  bool focusable_{false};
  bool accessible_{false};
  QString a11yLabel_, a11yRole_, a11yHint_, a11yValue_;
  bool a11yDisabled_{false}, a11ySelected_{false}, a11yChecked_{false};

  QString shownUri_;
  QString mediaType_;
  bool displayed_{false};
  bool loadPending_{false};
  bool bypassMemory_{false};
  quint64 generation_{0};

  std::unique_ptr<Layer> current_;
  std::unique_ptr<Layer> outgoing_;
  std::unique_ptr<Layer> placeholderLayer_;
  QVariantAnimation transitionAnimation_;
  qreal transitionProgress_{1.0};

  QtImageFetcher fetcher_;
  QtImageFetcher::Id mainFetch_{0};
  QtImageFetcher::Id placeholderFetch_{0};
  QFutureWatcher<DecodedImage> decodeWatcher_;
  QFutureWatcher<DecodedImage> placeholderWatcher_;
  quint64 decodeGeneration_{0};
  quint64 placeholderGeneration_{0};
  // What the decode in flight is for.
  QString pendingCacheType_;
  QString pendingUrl_;
  QString pendingKey_;
  bool pendingStore_{false};
};

}  // namespace rnqnx::views
