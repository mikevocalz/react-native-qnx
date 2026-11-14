#pragma once

#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/image/ImageDiskCache.h"
#include "react-native-qnx/views/image/QtImageFetcher.h"
#include <QImage>
#include <QObject>
#include <QString>

#include <functional>
#include <map>
#include <memory>

namespace rnqnx::runtime {

// Single-thread-affine decoded memory cache. Own on the Qt/JS event-loop thread.
// Header-bearing requests are deliberately isolated from the public URI cache.
class QtImageCache final : public QObject {
 public:
  using Headers = std::map<QByteArray, QByteArray>;
  using Completion = std::function<void(QSize, QString)>;
  QtImageCache();
  explicit QtImageCache(int budgetKiB, int timeoutMs = 30000);
  void load(const QString& uri, const Headers& headers, Completion complete);
  /// The general form: \a store false keeps the decoded image out of the
  /// shared memory cache. Disk caching follows request.policy.
  void load(const views::ImageRequest& request, bool store, Completion complete);
  /// Empties the shared decoded memory cache.
  void clearMemory();
  bool contains(const QString& uri) const;

 private:
  void decode(const QString& key, const QByteArray& bytes, bool cache,
              Completion complete);
  std::unique_ptr<views::DecodedImageCache> privateCache_;
  views::DecodedImageCache& images_;
  views::QtImageFetcher fetcher_;
};

}  // namespace rnqnx::runtime
