#include "react-native-qnx/views/image/ImageDecode.h"

#include <QBuffer>
#include <QImageReader>

namespace rnqnx::views {
namespace {

QString mediaTypeFor(const QByteArray& format) {
  const QByteArray lower = format.toLower();
  if (lower == "jpg" || lower == "jpeg") return QStringLiteral("image/jpeg");
  if (lower == "svg" || lower == "svgz") return QStringLiteral("image/svg+xml");
  if (lower.isEmpty()) return {};
  return QStringLiteral("image/") + QString::fromLatin1(lower);
}

}  // namespace

DecodedImage decodeImage(const QByteArray& bytes, QSize targetPixels, bool allowDownscaling,
                         ContentFit fit) {
  DecodedImage result;
  QBuffer buffer;
  buffer.setData(bytes);
  buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer);
  reader.setAutoTransform(true);
  const QSize size = reader.size();
  if (!size.isValid() ||
      static_cast<qint64>(size.width()) * size.height() > kMaxDecodedPixels) {
    result.error = QStringLiteral("Invalid image or image exceeds pixel limit");
    return result;
  }
  result.mediaType = mediaTypeFor(reader.format());
  result.animated = reader.supportsAnimation() && reader.imageCount() > 1;
  const bool wantsDownscale = allowDownscaling && !result.animated && targetPixels.isValid() &&
                              fit != ContentFit::None && fit != ContentFit::Fill &&
                              size.width() > targetPixels.width() * 2 &&
                              size.height() > targetPixels.height() * 2;
  if (wantsDownscale) {
    reader.setScaledSize(size.scaled(targetPixels, Qt::KeepAspectRatioByExpanding));
  }
  QImage image = reader.read();
  if (image.isNull()) {
    result.error = QStringLiteral("Could not decode image");
    return result;
  }
  result.downscaled = wantsDownscale;
  // EXIF rotation can swap the axes the reader reported.
  const bool swapped = (size.width() > size.height()) != (image.width() > image.height()) &&
                       size.width() != size.height();
  result.originalSize = wantsDownscale ? (swapped ? size.transposed() : size) : image.size();
  result.first = std::move(image);
  result.bytes = result.animated ? bytes : QByteArray();
  return result;
}

}  // namespace rnqnx::views
