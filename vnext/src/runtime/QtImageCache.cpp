#include "react-native-qnx/runtime/QtImageCache.h"
#include "react-native-qnx/views/image/ImageDecode.h"

#include <QThread>

#include <memory>

namespace rnqnx::runtime {

QtImageCache::QtImageCache()
    : images_(views::DecodedImageCache::shared()),
      fetcher_(30000, &views::ImageDiskCache::shared(), &views::ImageFetchQueue::shared()) {}

QtImageCache::QtImageCache(int budgetKiB, int timeoutMs)
    : privateCache_(std::make_unique<views::DecodedImageCache>(budgetKiB)),
      images_(*privateCache_), fetcher_(timeoutMs, nullptr) {}

bool QtImageCache::contains(const QString& uri) const {
  Q_ASSERT(QThread::currentThread() == thread());
  return images_.contains(uri);
}

void QtImageCache::decode(const QString& key, const QByteArray& bytes,
                          bool cache, std::uint64_t generation, ImageCompletion complete) {
  imageTasks_.run([bytes] {
    return views::decodeImage(bytes, {}, false, views::ContentFit::Cover);
  }, [this, key, cache, generation, complete = std::move(complete)](auto result) mutable {
    if (const auto* failure = std::get_if<views::ImageTaskError>(&result)) {
      complete({}, {}, failure->message);
      return;
    }
    auto decoded = std::get<views::DecodedImage>(std::move(result));
    if (!decoded.error.isEmpty()) {
      complete({}, {}, decoded.error);
      return;
    }
    const auto size = decoded.originalSize;
    // Keep playback bytes for cached Expo views, but deliver pixels regardless
    // of retention. The shared epoch prevents a cleared cache being refilled
    // by work that was admitted through any loader before clearMemoryCache.
    if (cache)
      images_.insert(key, decoded.first, std::move(decoded.mediaType),
                     std::move(decoded.bytes), generation);
    complete(std::move(decoded.first), size, {});
  });
}

void QtImageCache::load(const QString& uri, const Headers& headers,
                       Completion complete) {
  loadImage(uri, headers, [complete = std::move(complete)](QImage, QSize size, QString error) {
    complete(size, std::move(error));
  });
}

void QtImageCache::loadImage(const QString& uri, const Headers& headers,
                            ImageCompletion complete) {
  views::ImageRequest request;
  request.uri = uri;
  request.headers = headers;
  // The URI-only cache of React Native's Image is memory-only; a request that
  // carries headers is never stored (docs/image-loader.md).
  request.policy = views::ImageCachePolicy::Memory;
  loadImage(request, /*store=*/headers.empty(), std::move(complete));
}

void QtImageCache::load(const views::ImageRequest& request, bool store,
                        Completion complete) {
  loadImage(request, store, [complete = std::move(complete)](QImage, QSize size, QString error) {
    complete(size, std::move(error));
  });
}

void QtImageCache::loadImage(const views::ImageRequest& request, bool store,
                            ImageCompletion complete) {
  Q_ASSERT(QThread::currentThread() == thread());
  const QString key = request.storageKey();
  if (store && views::usesMemory(request.policy)) {
    if (auto image = images_.image(views::DecodedImageCache::key(key)); !image.isNull()) {
      const auto size = image.size();
      complete(std::move(image), size, {});
      return;
    }
  }
  const auto generation = images_.generation();
  fetcher_.fetch(request, {}, [this, key, store, generation, policy = request.policy,
                               complete = std::move(complete)](views::FetchResult result) mutable {
    if (!result.error.isEmpty()) {
      complete({}, {}, result.error);
      return;
    }
    // Respect private/no-store: never expose these in the URI-only cache.
    const bool cache = store && views::usesMemory(policy) && result.cacheable;
    decode(key, result.bytes, cache, generation, std::move(complete));
  });
}

void QtImageCache::clearMemory() {
  Q_ASSERT(QThread::currentThread() == thread());
  images_.clear();
}

}  // namespace rnqnx::runtime
