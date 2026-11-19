#pragma once

// The host's image loader, seen as an Image element's ImageProvider.
//
// It sends http(s), file, qrc and absolute-path sources to QtImageCache, the
// loader behind Image.getSize and Image.prefetch, so a bundled asset resolved
// by Image.resolveAssetSource (a file:// URI beside the bundle, or the dev
// server's /assets/ URL) takes the same route as the regular Image component.
// Decoded pixels come out of the shared DecodedImageCache, so a prefetched
// image is reused and one URI is fetched and decoded once. `data:` URIs are
// decoded in place, because QtImageCache accepts only base64 ones.

#include "react-native-qnx/runtime/QtImageCache.h"

#include <image/ImageProvider.h>

#include <QObject>
#include <QString>

#include <map>
#include <memory>
#include <vector>

namespace rnqnx::fabric {

class QtRnsvgImageProvider final : public QObject, public views::rnsvg::ImageProvider {
 public:
  // The process-wide provider, parented to the QCoreApplication so it is
  // destroyed with it. Call on the Qt GUI thread, the thread that owns
  // QtImageCache's network manager.
  static QtRnsvgImageProvider& shared();

  // For tests that bring their own cache.
  explicit QtRnsvgImageProvider(runtime::QtImageCache& cache, QObject* parent = nullptr);
  ~QtRnsvgImageProvider() override;

  std::unique_ptr<views::rnsvg::ImageRequest> load(const QString& uri, Completion completion) override;

  // Requests that have been sent to the network and not yet answered, by URI.
  int pendingFetches() const { return int(fetches_.size()); }

 private:
  struct Waiter;
  class Handle;
  void finish(const QString& uri, const QString& error);
  void forget(const QString& uri, const std::shared_ptr<Waiter>& waiter);

  runtime::QtImageCache& cache_;
  // Everyone waiting on one URI shares one fetch.
  std::map<QString, std::vector<std::shared_ptr<Waiter>>> fetches_;
};

}  // namespace rnqnx::fabric
