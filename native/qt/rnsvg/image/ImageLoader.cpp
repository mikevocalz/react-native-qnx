#include "ImageLoader.h"

#include <utility>

namespace rnqnx::views::rnsvg {

struct ImageLoader::Core {
  ImageState state{ImageState::Idle};
  QString uri;
  QImage image;
  QString error;
  std::uint64_t generation{0};
  SettledHandler onSettled;
};

ImageLoader::ImageLoader(ImageProvider& provider, SettledHandler onSettled)
    : provider_(provider), core_(std::make_shared<Core>()) {
  core_->onSettled = std::move(onSettled);
}

// The core is released here; a completion that still holds a weak reference to
// it finds nothing and returns.
ImageLoader::~ImageLoader() = default;

ImageState ImageLoader::state() const { return core_->state; }
const QString& ImageLoader::uri() const { return core_->uri; }
const QImage& ImageLoader::image() const { return core_->image; }
const QString& ImageLoader::error() const { return core_->error; }
std::uint64_t ImageLoader::generation() const { return core_->generation; }

void ImageLoader::setSource(const QString& uri) {
  if (uri == core_->uri && core_->state != ImageState::Idle) return;
  if (uri.isEmpty() && core_->state == ImageState::Idle) return;

  request_.reset();  // cancels the previous load
  const std::uint64_t generation = ++core_->generation;
  core_->uri = uri;
  core_->image = QImage();
  core_->error.clear();
  if (uri.isEmpty()) {
    core_->state = ImageState::Idle;
    return;
  }
  core_->state = ImageState::Loading;

  std::weak_ptr<Core> weak = core_;
  auto handle = provider_.load(uri, [this, weak, generation](ImageLoadResult result) {
    // `this` is only touched after the weak reference locks, which means the
    // loader is still alive.
    if (auto core = weak.lock()) settle(core, generation, std::move(result));
  });
  // A completion that ran inside load() may have destroyed this loader or
  // started another load through the settled handler. `this` is touched only
  // if the core is still alive, and the handle is kept only if it is still the
  // current one.
  const auto core = weak.lock();
  if (core && core->generation == generation) request_ = std::move(handle);
}

void ImageLoader::settle(const std::shared_ptr<Core>& core, std::uint64_t generation, ImageLoadResult result) {
  if (generation != core->generation || core->state != ImageState::Loading) return;
  if (auto* image = std::get_if<QImage>(&result)) {
    if (image->isNull()) {
      core->error = QStringLiteral("The decoded image is empty");
    } else if (image->width() > kMaxImageDimension || image->height() > kMaxImageDimension) {
      core->error = QStringLiteral("Image exceeds %1x%1 pixels").arg(kMaxImageDimension);
    } else {
      core->image = std::move(*image);
    }
  } else {
    core->error = std::get<ImageLoadFailure>(result).message;
    if (core->error.isEmpty()) core->error = QStringLiteral("Image load failed");
  }
  core->state = core->error.isEmpty() ? ImageState::Loaded : ImageState::Failed;
  // Copy the handler: it may destroy the loader, and with it the core.
  const SettledHandler handler = core->onSettled;
  if (handler) handler(*this);
}

}  // namespace rnqnx::views::rnsvg
