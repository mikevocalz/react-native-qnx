#include "react-native-qnx/views/QnxImageProvider.h"
#include "react-native-qnx/views/DecodedImageCache.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QSize>

namespace rnqnx::views {

const char* QnxImageProvider::providerId() {
  return "rnqnx";
}

QnxImageProvider::QnxImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QString QnxImageProvider::registerDataUri(const QString& dataUri) {
  if (!dataUri.startsWith(QLatin1String("data:"))) {
    return {};
  }

  const int comma = dataUri.indexOf(QLatin1Char(','));
  if (comma < 0) {
    return {};
  }

  const QStringView header = QStringView(dataUri).left(comma);
  if (!header.contains(QLatin1String(";base64"))) {
    // Percent-encoded data URIs are legal but React Native does not emit them,
    // so they are declined rather than half-supported.
    return {};
  }

  // The digest keys the cache, so the same image referenced twice is decoded
  // once and both items share it.
  const QString key = QString::fromLatin1(
      QCryptographicHash::hash(dataUri.toUtf8(), QCryptographicHash::Sha1)
          .toHex());

  if (!images_.contains(key)) {
    const QByteArray payload = QByteArray::fromBase64(
        dataUri.mid(comma + 1).toLatin1());
    QImage image;
    if (payload.isEmpty() || !image.loadFromData(payload)) {
      return {};
    }
    images_.insert(key, image);
  }

  return QStringLiteral("image://%1/%2")
      .arg(QLatin1String(providerId()), key);
}

QImage QnxImageProvider::requestImage(
    const QString& id,
    QSize* size,
    const QSize& requestedSize) {
  const QImage image = id.startsWith(QLatin1String("cache/"))
      ? DecodedImageCache::shared().image(id.mid(6))
      : images_.value(id);
  if (image.isNull()) {
    return image;
  }

  if (size != nullptr) {
    *size = image.size();
  }
  if (requestedSize.isValid() && requestedSize != image.size()) {
    return image.scaled(
        requestedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }
  return image;
}

}  // namespace rnqnx::views
