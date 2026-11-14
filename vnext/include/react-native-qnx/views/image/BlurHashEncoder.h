#pragma once

#include <QImage>
#include <QString>

namespace rnqnx::views {

/// Encodes \a image as a BlurHash (https://blurha.sh) with \a componentsX by
/// \a componentsY components, each 1 to 9. An image longer than 100 px on its
/// long side is scaled down first; the encoder's cost grows with the pixel
/// count and the extra detail never reaches a 9x9 hash. Returns an empty
/// string for a null image or components outside 1 to 9.
QString encodeBlurHash(const QImage& image, int componentsX, int componentsY);

}  // namespace rnqnx::views
