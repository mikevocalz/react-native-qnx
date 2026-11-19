#include "ImageElementProps.h"

#include "ImageSource.h"

namespace rnqnx::views::rnsvg {

void carryImageLoadState(ElementProps& next, const ElementProps& previous) {
  if (!next.imageUri.isEmpty() && next.imageUri == previous.imageUri) {
    next.image = previous.image;
    next.imageState = previous.imageState;
    return;
  }
  next.image = QImage();
  next.imageState = toInt(next.imageUri.isEmpty() ? ImageState::Idle : ImageState::Loading);
}

}  // namespace rnqnx::views::rnsvg
