#pragma once

#include "ImageProvider.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace rnqnx::views::rnsvg {

// The load state of one Image element: idle, loading, loaded or failed for the
// current source.
//
// setSource() starts a load and bumps a generation counter. A completion that
// belongs to an older generation is dropped, so a prop change to a new URI
// never lets the old image land. The completion closure holds only a weak
// reference to the shared state, so a loader destroyed mid-flight is never
// called back.
//
// Thread-affine: use it on the thread that owns the provider.
class ImageLoader {
 public:
  // Called when a load settles (Loaded or Failed), never for Idle or Loading.
  using SettledHandler = std::function<void(const ImageLoader&)>;

  ImageLoader(ImageProvider& provider, SettledHandler onSettled);
  ~ImageLoader();
  ImageLoader(const ImageLoader&) = delete;
  ImageLoader& operator=(const ImageLoader&) = delete;

  // An unchanged URI is a no-op, including after a failure: a failed source is
  // not retried on every commit. An empty URI cancels and goes Idle.
  void setSource(const QString& uri);

  ImageState state() const;
  const QString& uri() const;
  const QImage& image() const;
  const QString& error() const;
  // Number of setSource() calls that started a load.
  std::uint64_t generation() const;

 private:
  struct Core;
  void settle(const std::shared_ptr<Core>& core, std::uint64_t generation, ImageLoadResult result);

  ImageProvider& provider_;
  std::shared_ptr<Core> core_;
  std::unique_ptr<ImageRequest> request_;
};

}  // namespace rnqnx::views::rnsvg
