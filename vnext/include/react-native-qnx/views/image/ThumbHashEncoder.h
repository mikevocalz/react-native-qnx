#pragma once

#include <QByteArray>
#include <QImage>

namespace rnqnx::views {

/// Encodes \a image as a ThumbHash (https://evanw.github.io/thumbhash). An
/// image over 100 px on either side is scaled down first, the limit of the
/// reference encoder. Returns an empty array for a null image.
QByteArray encodeThumbHash(const QImage& image);

}  // namespace rnqnx::views
