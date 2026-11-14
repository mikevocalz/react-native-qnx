#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

namespace rnqnx::views {

/// Decodes a ThumbHash (https://evanw.github.io/thumbhash) given as base64.
/// The image is about 32 px on its long side with the hash's aspect ratio and
/// has alpha when the hash says so. Null for a hash that is too short.
QImage decodeThumbHash(const QString& base64);
QImage decodeThumbHash(const QByteArray& hash);

}  // namespace rnqnx::views
