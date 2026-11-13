#include "SvgImageSource.h"

#include <QBuffer>
#include <QImageReader>
#include <QUrl>

namespace rnqnx::views::rnsvg::testing {
namespace {

constexpr qsizetype kMaxDataUriChars = 16 * 1024 * 1024;
constexpr int kMaxDimension = 8192;

QImage decode(const QByteArray& bytes) {
  QBuffer buffer;
  buffer.setData(bytes);
  buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer);
  const QSize size = reader.size();
  if (size.isValid() && (size.width() > kMaxDimension || size.height() > kMaxDimension)) return {};
  return reader.read();
}

// ImageState::Loaded is 2 and Failed 3 (image/ImageSource.h); the old model has no state.
void markDecoded(ElementProps& e) {
#if RNQNX_RNSVG_FULL_MODEL
  e.imageState = e.image.isNull() ? 3 : 2;
#else
  (void)e;
#endif
}

}  // namespace

void loadImageSource(ElementProps& e, const QString& href) {
  e.imageUri = href;
  if (href.startsWith(QStringLiteral("data:"))) {
    const qsizetype comma = href.indexOf(QLatin1Char(','));
    if (comma < 0 || href.size() > kMaxDataUriChars) return;
    const QByteArray payload = href.mid(comma + 1).toUtf8();
    const bool base64 = href.left(comma).contains(QStringLiteral(";base64"));
    e.image = decode(base64 ? QByteArray::fromBase64(payload) : QByteArray::fromPercentEncoding(payload));
    markDecoded(e);
  } else if (href.startsWith(QStringLiteral("file://"))) {
    e.image = QImage(QUrl(href).toLocalFile());
    markDecoded(e);
  } else if (href.startsWith(QLatin1Char('/'))) {
    e.image = QImage(href);
    markDecoded(e);
  }
}

}  // namespace rnqnx::views::rnsvg::testing
