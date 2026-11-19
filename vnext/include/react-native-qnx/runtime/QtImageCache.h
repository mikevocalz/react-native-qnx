#pragma once

#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/image/ImageDiskCache.h"
#include "react-native-qnx/views/image/ImageTaskQueue.h"
#include "react-native-qnx/views/image/QtImageFetcher.h"
#include <QImage>
#include <QObject>
#include <QString>

#include <functional>
#include <cstdint>
#include <map>
#include <memory>

namespace rnqnx::runtime {

// Single-thread-affine decoded memory cache. Own on the Qt/JS event-loop thread.
// Header-bearing requests are deliberately isolated from the public URI cache.
class QtImageCache final : public QObject {
 public:
  using Headers = std::map<QByteArray, QByteArray>;
  using Completion = std::function<void(QSize, QString)>;
  // The decoded pixels are delivered even when cache policy, budget or a
  // concurrent clear prevents retaining them. Completion stays on the owner.
  using ImageCompletion = std::function<void(QImage, QSize, QString)>;
  QtImageCache();
  explicit QtImageCache(int budgetKiB, int timeoutMs = 30000);
  void load(const QString& uri, const Headers& headers, Completion complete);
  /// The general form: \a store false keeps the decoded image out of the
  /// shared memory cache. Disk caching follows request.policy.
  void load(const views::ImageRequest& request, bool store, Completion complete);
  void loadImage(const QString& uri, const Headers& headers, ImageCompletion complete);
  void loadImage(const views::ImageRequest& request, bool store, ImageCompletion complete);
  /// Empties the shared decoded memory cache.
  void clearMemory();
  bool contains(const QString& uri) const;

 private:
  void decode(const QString& key, const QByteArray& bytes, bool cache,
              std::uint64_t generation, ImageCompletion complete);
  std::unique_ptr<views::DecodedImageCache> privateCache_;
  views::DecodedImageCache& images_;
  // Fetch callbacks may enqueue decode work, so destroy the fetcher first.
  views::ImageTaskQueue imageTasks_;
  views::QtImageFetcher fetcher_;
};

}  // namespace rnqnx::runtime
