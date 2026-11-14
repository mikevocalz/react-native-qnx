#include "ImageBinding.h"

#include "../RnsvgItems.h"

namespace rnqnx::views::rnsvg {

RnsvgImageBinding::RnsvgImageBinding(RnsvgElementItem& element, ImageProvider& provider)
    : QObject(&element),
      element_(&element),
      loader_(provider, [this](const ImageLoader& loader) { onSettled(loader); }) {}

RnsvgImageBinding& RnsvgImageBinding::forElement(RnsvgElementItem& element, ImageProvider& provider) {
  if (auto* found = existing(element)) return *found;
  return *new RnsvgImageBinding(element, provider);
}

RnsvgImageBinding* RnsvgImageBinding::existing(const RnsvgElementItem& element) {
  return element.findChild<RnsvgImageBinding*>(QString(), Qt::FindDirectChildrenOnly);
}

void RnsvgImageBinding::setSource(const QString& uri) { loader_.setSource(uri); }

void RnsvgImageBinding::onSettled(const ImageLoader& loader) {
  if (!element_) return;
  // A commit may have moved the element to another URI between the load
  // starting and settling. The loader already drops that case by generation;
  // this keeps the element's props the authority on what it shows.
  if (element_->props().imageUri != loader.uri()) return;

  ElementProps next = element_->props();
  next.image = loader.image();
  next.imageState = toInt(loader.state());
  element_->setProps(std::move(next));

  if (loader.state() == ImageState::Loaded) {
    // Queued on this object, so the call is dropped with the element. React
    // reads onLoad as an asynchronous event, and a handler that sets state must
    // not run inside the mount that started the load.
    QMetaObject::invokeMethod(
        this,
        [this, event = ImageLoadedEvent{loader.uri(), loader.image().size()}] {
          if (!element_ || element_->props().imageUri != event.uri || !onLoaded_) return;
          // Copy: the callback may replace itself by committing new props.
          const LoadedCallback callback = onLoaded_;
          callback(event);
        },
        Qt::QueuedConnection);
  }
}

}  // namespace rnqnx::views::rnsvg
