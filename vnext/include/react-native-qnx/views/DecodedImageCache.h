#pragma once

#include <QByteArray>
#include <QCache>
#include <QCryptographicHash>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

#include <algorithm>
#include <deque>
#include <utility>

namespace rnqnx::views {

// Shared decoded images bridge Image.prefetch to Qt Quick's image provider.
// QImage copies are implicitly shared; the mutex also permits provider reads
// from Qt's image-loading thread. The process cache is bounded and memory-only.
class DecodedImageCache {
 public:
  struct Entry {
    QImage image;
    QString mediaType;
    // A decoded first frame alone cannot restore playback in an Expo view.
    QByteArray animationBytes;
  };
  explicit DecodedImageCache(int budgetKiB = 32 * 1024) : images_(budgetKiB) {}
  static DecodedImageCache& shared() {
    static DecodedImageCache cache;
    return cache;
  }
  static QString key(const QString& uri) {
    return QString::fromLatin1(QCryptographicHash::hash(
        uri.toUtf8(), QCryptographicHash::Sha256).toHex());
  }
  Entry entry(const QString& key) {
    QMutexLocker lock(&mutex_);
    const auto* found = images_.object(key);
    return found ? *found : Entry{};
  }
  QImage image(const QString& key) { return entry(key).image; }
  /// Drops every decoded image.
  void clear() {
    QMutexLocker lock(&mutex_);
    images_.clear();
    order_.clear();
  }
  /// Whether an image stored under \a uri (or a storage key) is held.
  bool contains(const QString& uri) {
    QMutexLocker lock(&mutex_);
    return images_.contains(key(uri));
  }
  void insert(const QString& uri, QImage image, QString mediaType = {},
              QByteArray animationBytes = {}) {
    const auto cost = (image.sizeInBytes() + animationBytes.size() + 1023) / 1024;
    QMutexLocker lock(&mutex_);
    if (cost <= images_.maxCost()) {
      const QString hashed = key(uri);
      images_.insert(hashed, new Entry{std::move(image), std::move(mediaType), std::move(animationBytes)},
                     static_cast<int>(cost));
      noteInsertLocked(hashed);
      trimToCountLocked();
    }
  }
  /// expo-image's configureCache: the cost ceiling in KiB (\a maxCostKiB <= 0
  /// keeps the current one) and the most pictures held (\a maxCount <= 0 means
  /// no limit). Beyond the count the oldest inserted pictures go first.
  void configure(int maxCostKiB, int maxCount) {
    QMutexLocker lock(&mutex_);
    if (maxCostKiB > 0) images_.setMaxCost(maxCostKiB);
    maxCount_ = std::max(0, maxCount);
    trimToCountLocked();
  }
  int maxCostKiB() {
    QMutexLocker lock(&mutex_);
    return images_.maxCost();
  }
  int count() {
    QMutexLocker lock(&mutex_);
    return images_.count();
  }
 private:
  // order_ lists keys oldest first. QCache evicts on cost by itself, so it can
  // hold keys that are gone; they are skipped and pruned.
  void noteInsertLocked(const QString& hashed) {
    const auto stale = std::find(order_.begin(), order_.end(), hashed);
    if (stale != order_.end()) order_.erase(stale);
    order_.push_back(hashed);
    if (order_.size() > static_cast<size_t>(images_.count()) * 2 + 64) {
      std::deque<QString> live;
      for (const QString& key : order_) {
        if (images_.contains(key)) live.push_back(key);
      }
      order_.swap(live);
    }
  }
  void trimToCountLocked() {
    while (maxCount_ > 0 && images_.count() > maxCount_) {
      if (!order_.empty()) {
        images_.remove(order_.front());
        order_.pop_front();
      } else {
        images_.remove(images_.keys().constFirst());
      }
    }
  }

  QMutex mutex_;
  QCache<QString, Entry> images_;
  std::deque<QString> order_;
  int maxCount_{0};
};

}  // namespace rnqnx::views
