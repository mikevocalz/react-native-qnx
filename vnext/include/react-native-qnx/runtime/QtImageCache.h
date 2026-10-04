#pragma once

#include "react-native-qnx/views/DecodedImageCache.h"
#include <QImage>
#include <QNetworkAccessManager>
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
  bool contains(const QString& uri) const;

 private:
  void decode(const QString& uri, const QByteArray& bytes, bool cache,
              Completion complete);
  QNetworkAccessManager network_;
  std::unique_ptr<views::DecodedImageCache> privateCache_;
  views::DecodedImageCache& images_;
  int timeoutMs_;
};

}  // namespace rnqnx::runtime
