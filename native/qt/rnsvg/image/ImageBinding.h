#pragma once

#include "ImageLoader.h"

#include <QObject>
#include <QPointer>
#include <QSize>
#include <functional>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

struct ImageLoadedEvent {
  QString uri;
  QSize size;
};

// Connects one Image element to a provider. It is a QObject child of the
// element, so it is deleted with it, and its loader then drops any load still
// in flight.
//
// When a load settles it copies the element's props, sets `image` and
// `imageState`, and calls setProps(), which asks the root for one repaint.
// Starting a load repaints nothing: the commit that changed the URI already
// did.
class RnsvgImageBinding final : public QObject {
  Q_OBJECT

 public:
  using LoadedCallback = std::function<void(const ImageLoadedEvent&)>;

  // The element's binding, created on first use. `provider` must outlive it.
  static RnsvgImageBinding& forElement(RnsvgElementItem& element, ImageProvider& provider);
  // The element's binding, or nullptr if it has none.
  static RnsvgImageBinding* existing(const RnsvgElementItem& element);

  // Call after the element's props have the new `imageUri` (see
  // carryImageLoadState). A URI the loader already has is left alone.
  void setSource(const QString& uri);

  // Runs once per successful load, after the element has been repainted, from
  // a queued call on the next turn of the event loop (never inside the commit
  // that started the load, even when the provider answered at once, and never
  // after the element is gone). It is skipped if the element has moved to
  // another URI by then. Replaced on every commit so it always holds the latest
  // event emitter.
  void setLoadedCallback(LoadedCallback callback) { onLoaded_ = std::move(callback); }

  const ImageLoader& loader() const { return loader_; }

 private:
  RnsvgImageBinding(RnsvgElementItem& element, ImageProvider& provider);
  void onSettled(const ImageLoader& loader);

  QPointer<RnsvgElementItem> element_;
  ImageLoader loader_;
  LoadedCallback onLoaded_;
};

}  // namespace rnqnx::views::rnsvg
