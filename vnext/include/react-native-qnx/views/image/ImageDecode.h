#pragma once

#include "react-native-qnx/views/image/ImageLayout.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>

namespace rnqnx::views {

/// The outcome of decoding encoded image bytes. Safe to build on a worker
/// thread: it touches no Qt Quick object.
struct DecodedImage {
  /// The first frame (the only one for a still image). Null on error.
  QImage first;
  /// The picture's own size in pixels, before any downscaling.
  QSize originalSize;
  bool animated{false};
  /// True when `first` is smaller than `originalSize`; such an image must not
  /// enter the shared decoded cache, where other views expect full size.
  bool downscaled{false};
  /// The encoded bytes, kept so an animated image can be played.
  QByteArray bytes;
  QString mediaType;
  QString error;
};

constexpr qint64 kMaxDecodedPixels = 16 * 1024 * 1024;

/// Decodes \a bytes. When \a allowDownscaling is set, the image is much larger
/// than \a targetPixels (twice in both dimensions) and \a fit is neither
/// `none` nor `fill` (the modes that need every pixel), it is decoded at the
/// smallest size that still covers the target.
DecodedImage decodeImage(const QByteArray& bytes, QSize targetPixels, bool allowDownscaling,
                         ContentFit fit);

}  // namespace rnqnx::views
