#include "react-native-qnx/runtime/QtImageCache.h"

#include <QBuffer>
#include <QImageReader>
#include <QThread>

#include <memory>

namespace rnqnx::runtime {
namespace {
constexpr qint64 kMaxPixels = 16 * 1024 * 1024;
}

QtImageCache::QtImageCache()
    : images_(views::DecodedImageCache::shared()),
      fetcher_(30000, &views::ImageDiskCache::shared()) {}

QtImageCache::QtImageCache(int budgetKiB, int timeoutMs)
    : privateCache_(std::make_unique<views::DecodedImageCache>(budgetKiB)),
      images_(*privateCache_), fetcher_(timeoutMs, nullptr) {}

bool QtImageCache::contains(const QString& uri) const {
  Q_ASSERT(QThread::currentThread() == thread());
  return images_.contains(uri);
}

void QtImageCache::decode(const QString& key, const QByteArray& bytes,
                          bool cache, Completion complete) {
  QBuffer buffer;
  buffer.setData(bytes);
  buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer);
  const QSize dimensions = reader.size();
  if (!dimensions.isValid() ||
      static_cast<qint64>(dimensions.width()) * dimensions.height() > kMaxPixels) {
    complete({}, QStringLiteral("Invalid image or image exceeds pixel limit"));
    return;
  }
  QImage image = reader.read();
  if (image.isNull()) {
    complete({}, QStringLiteral("Could not decode image"));
    return;
  }
  const auto size = image.size();
  if (cache) images_.insert(key, std::move(image));
  complete(size, {});
}

void QtImageCache::load(const QString& uri, const Headers& headers,
                       Completion complete) {
  views::ImageRequest request;
  request.uri = uri;
  request.headers = headers;
  // The URI-only cache of React Native's Image is memory-only; a request that
  // carries headers is never stored (docs/image-loader.md).
  request.policy = views::ImageCachePolicy::Memory;
  load(request, /*store=*/headers.empty(), std::move(complete));
}

void QtImageCache::load(const views::ImageRequest& request, bool store,
                        Completion complete) {
  Q_ASSERT(QThread::currentThread() == thread());
  const QString key = request.storageKey();
  if (store && views::usesMemory(request.policy)) {
    if (const auto image = images_.image(views::DecodedImageCache::key(key)); !image.isNull()) {
      complete(image.size(), {});
      return;
    }
  }
  fetcher_.fetch(request, {}, [this, key, store, policy = request.policy,
                               complete = std::move(complete)](views::FetchResult result) mutable {
    if (!result.error.isEmpty()) {
      complete({}, result.error);
      return;
    }
    // Respect private/no-store: never expose these in the URI-only cache.
    const bool cache = store && views::usesMemory(policy) && result.cacheable;
    decode(key, result.bytes, cache, std::move(complete));
  });
}

void QtImageCache::clearMemory() {
  Q_ASSERT(QThread::currentThread() == thread());
  images_.clear();
}

}  // namespace rnqnx::runtime
