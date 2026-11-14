#pragma once

#include "Style.h"

#include <QImage>
#include <QString>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

class Scene;

// <Image>: draws the element's decoded image into its box with
// preserveAspectRatio. Loading and decoding happen elsewhere (image/); this
// draws whatever image the element currently holds.
class ImageRenderer {
 public:
  explicit ImageRenderer(Scene& scene) : scene_(scene) {}

  void draw(const RnsvgElementItem& e, const Frame& f, int depth);

  // Synchronous decode of a `data:` URI or a local file, for feImage. Null for
  // anything else, including a source that is still loading.
  static QImage decodeLocal(const QString& href);

 private:
  Scene& scene_;
};

}  // namespace rnqnx::views::rnsvg
