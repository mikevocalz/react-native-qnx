#pragma once

#include "react-native-qnx/views/image/ImageFetchQueue.h"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <map>
#include <cstddef>
#include <memory>

namespace rnqnx::views {

class ImageDiskCache;

/// expo-image's cachePolicy, as the views and the loader apply it.
enum class ImageCachePolicy { None, Disk, Memory, MemoryDisk };

ImageCachePolicy parseCachePolicy(const QString& text);
inline bool usesMemory(ImageCachePolicy policy) {
  return policy == ImageCachePolicy::Memory || policy == ImageCachePolicy::MemoryDisk;
}
inline bool usesDisk(ImageCachePolicy policy) {
  return policy == ImageCachePolicy::Disk || policy == ImageCachePolicy::MemoryDisk;
}

using ImageHeaders = std::map<QByteArray, QByteArray>;

/// One image to fetch. The cache key is the caller's `cacheKey` when it gave
/// one; otherwise the URI, with the headers folded in so the same URI fetched
/// with different headers is cached separately.
struct ImageRequest {
  QString uri;
  ImageHeaders headers;
  QString cacheKey;
  ImageCachePolicy policy{ImageCachePolicy::MemoryDisk};
  /// expo-image's `priority`: which waiting HTTP load starts first.
  ImagePriority priority{ImagePriority::Normal};

  QString storageKey() const;
};

enum class ImageOrigin { Network, Disk, Local };

struct FetchResult {
  QByteArray bytes;
  QString error;  ///< Empty on success.
  ImageOrigin origin{ImageOrigin::Network};
  /// False when the response said private or no-store: keep it off every cache.
  bool cacheable{true};
};

/// Gets the encoded bytes of an image: from the disk cache, over HTTP(S) with
/// the request's headers, or from a file, qrc or base64 data URI. It owns the
/// network manager and every in-flight request, so destroying it (or calling
/// cancel) abandons the work and never calls back afterwards. HTTP loads run
/// through an ImageFetchQueue: at most that queue's limit at once, the rest
/// wait in priority order. Fetchers given the same queue (the views and the
/// ImageLoader share ImageFetchQueue::shared()) order their loads together.
/// Single-threaded: use it on the thread it was created on.
class QtImageFetcher final : public QObject {
 public:
  using Progress = std::function<void(qint64 received, qint64 total)>;
  using Done = std::function<void(FetchResult)>;
  using Id = std::uint64_t;

  /// \a queue orders this fetcher's HTTP loads; null gives the fetcher a queue
  /// of its own with \a maxConcurrent slots.
  explicit QtImageFetcher(int timeoutMs = 30000, ImageDiskCache* disk = nullptr,
                          ImageFetchQueue* queue = nullptr,
                          int maxConcurrent = ImageFetchQueue::kSharedConcurrency);
  ~QtImageFetcher() override;

  /// Starts \a request. \a done runs exactly once, unless the fetch is
  /// cancelled first. Returns an id for cancel().
  Id fetch(const ImageRequest& request, Progress progress, Done done);
  void cancel(Id id);

  /// HTTP loads on the wire, and loads waiting for a free slot.
  int activeRequests() const { return static_cast<int>(started_); }
  int queuedRequests() const { return static_cast<int>(pending_.size() - started_); }

  static constexpr qint64 kMaxEncodedBytes = 16 * 1024 * 1024;

 private:
  struct Pending;
  void startHttp(Id id);
  void finishHttp(Id id, Pending& pending);
  FetchResult readLocal(const QString& uri) const;

  QNetworkAccessManager network_;
  ImageDiskCache* disk_;
  int timeoutMs_;
  ImageFetchQueue ownQueue_;
  ImageFetchQueue* queue_;
  std::size_t started_{0};
  Id nextId_{1};
  std::map<Id, std::shared_ptr<Pending>> pending_;
};

}  // namespace rnqnx::views
