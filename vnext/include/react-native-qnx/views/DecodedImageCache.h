#pragma once

#include <QCache>
#include <QCryptographicHash>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

namespace rnqnx::views {

// Shared decoded images bridge Image.prefetch to Qt Quick's image provider.
// QImage copies are implicitly shared; the mutex also permits provider reads
// from Qt's image-loading thread. The process cache is bounded and memory-only.
class DecodedImageCache {
 public:
  explicit DecodedImageCache(int budgetKiB = 32 * 1024) : images_(budgetKiB) {}
  static DecodedImageCache& shared() {
    static DecodedImageCache cache;
    return cache;
  }
  static QString key(const QString& uri) {
    return QString::fromLatin1(QCryptographicHash::hash(
        uri.toUtf8(), QCryptographicHash::Sha256).toHex());
  }
  QImage image(const QString& key) {
    QMutexLocker lock(&mutex_);
    const auto* found = images_.object(key);
    return found ? *found : QImage{};
  }
  bool contains(const QString& uri) {
    QMutexLocker lock(&mutex_);
    return images_.contains(key(uri));
  }
  void insert(const QString& uri, QImage image) {
    const auto cost = (image.sizeInBytes() + 1023) / 1024;
    QMutexLocker lock(&mutex_);
    if (cost <= images_.maxCost()) {
      images_.insert(key(uri), new QImage(std::move(image)), static_cast<int>(cost));
    }
  }
 private:
  QMutex mutex_;
  QCache<QString, QImage> images_;
};

}  // namespace rnqnx::views
