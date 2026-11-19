#pragma once

#include "ModelAdapter.h"

namespace rnqnx::views::rnsvg::testing {

// Sets e.imageUri and decodes data: URIs and local files into e.image, as the
// Fabric bridge does when it receives an Image element. Other URIs stay
// undecoded. Decoding is bounded: a data URI over 16 MiB of text, or a raster
// over 8192 pixels on a side, gives an empty image.
void loadImageSource(ElementProps& e, const QString& href);

}  // namespace rnqnx::views::rnsvg::testing
