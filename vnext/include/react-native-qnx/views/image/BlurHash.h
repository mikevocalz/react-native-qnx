#pragma once

#include <QImage>
#include <QString>

namespace rnqnx::views {

/// Decodes a BlurHash (https://blurha.sh) to a \a width x \a height image.
/// Returns a null image for a malformed or truncated hash.
QImage decodeBlurHash(const QString& hash, int width, int height);

}  // namespace rnqnx::views
