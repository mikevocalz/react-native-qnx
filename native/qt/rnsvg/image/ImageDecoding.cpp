#include "ImageDecoding.h"

#include <QBuffer>
#include <QImageReader>
#include <QSize>
#include <QUrl>

namespace rnqnx::views::rnsvg {

bool svgDecodingAvailable() {
  return QImageReader::supportedImageFormats().contains(QByteArrayLiteral("svg"));
}

QString svgUnavailableMessage() {
  return QStringLiteral(
      "SVG images need Qt's svg image-format plugin (qsvg), which is not installed; "
      "this renderer does not link Qt6::Svg");
}

bool looksLikeSvg(const QByteArray& bytes) {
  // Skip a UTF-8 byte order mark and leading whitespace, then look for an XML
  // prolog or a root element within the first kilobyte.
  const QByteArray head = bytes.left(1024).trimmed();
  const QByteArray text = head.startsWith("\xEF\xBB\xBF") ? head.mid(3).trimmed() : head;
  return text.startsWith("<?xml") || text.startsWith("<svg") || text.startsWith("<!--") ||
         text.startsWith("<!DOCTYPE svg") || text.contains("<svg");
}

bool uriNamesSvg(const QString& uri) {
  if (uri.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
    const qsizetype comma = uri.indexOf(QLatin1Char(','));
    const QString header = uri.left(comma < 0 ? 64 : comma);
    return header.contains(QStringLiteral("image/svg"), Qt::CaseInsensitive);
  }
  // Compare the path, so a query string after ".svg" still counts.
  const QString path = QUrl(uri).path();
  return path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive) ||
         path.endsWith(QStringLiteral(".svgz"), Qt::CaseInsensitive);
}

ImageLoadResult decodeImageBytes(const QByteArray& bytes) {
  if (bytes.isEmpty()) return ImageLoadFailure{QStringLiteral("Image data is empty")};
  QBuffer buffer;
  buffer.setData(bytes);
  if (!buffer.open(QIODevice::ReadOnly)) return ImageLoadFailure{QStringLiteral("Could not read image data")};
  QImageReader reader(&buffer);
  const bool svg = reader.format() == "svg" || reader.format() == "svgz";
  const QSize size = reader.size();
  if (size.isValid() && (size.width() > kMaxImageDimension || size.height() > kMaxImageDimension)) {
    if (!svg) {
      return ImageLoadFailure{QStringLiteral("Image exceeds %1x%1 pixels").arg(kMaxImageDimension)};
    }
    // A vector image has no pixel size of its own, so scale it into the bound.
    reader.setScaledSize(size.scaled(kMaxImageDimension, kMaxImageDimension, Qt::KeepAspectRatio));
  }
  QImage image = reader.read();
  if (image.isNull()) {
    if (looksLikeSvg(bytes) && !svgDecodingAvailable()) return ImageLoadFailure{svgUnavailableMessage()};
    return ImageLoadFailure{QStringLiteral("Could not decode image: %1").arg(reader.errorString())};
  }
  if (image.width() > kMaxImageDimension || image.height() > kMaxImageDimension) {
    return ImageLoadFailure{QStringLiteral("Image exceeds %1x%1 pixels").arg(kMaxImageDimension)};
  }
  return image;
}

ImageLoadResult decodeDataUri(const QString& uri) {
  if (uri.size() > kMaxDataUriChars) return ImageLoadFailure{QStringLiteral("Image data URI is too large")};
  const qsizetype comma = uri.indexOf(QLatin1Char(','));
  if (!uri.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive) || comma < 0) {
    return ImageLoadFailure{QStringLiteral("Malformed data URI")};
  }
  const QString header = uri.left(comma);
  const QByteArray payload = uri.mid(comma + 1).toUtf8();
  const bool base64 = header.contains(QStringLiteral(";base64"), Qt::CaseInsensitive);
  const QByteArray bytes = base64 ? QByteArray::fromBase64(payload) : QByteArray::fromPercentEncoding(payload);
  return decodeImageBytes(bytes);
}

}  // namespace rnqnx::views::rnsvg
