#include "react-native-qnx/runtime/QtImageCache.h"

#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace rnqnx::runtime {
namespace {
constexpr qint64 kMaxEncodedBytes = 16 * 1024 * 1024;
constexpr qint64 kMaxPixels = 16 * 1024 * 1024;
}

QtImageCache::QtImageCache()
    : images_(views::DecodedImageCache::shared()), timeoutMs_(30000) {}

QtImageCache::QtImageCache(int budgetKiB, int timeoutMs)
    : privateCache_(std::make_unique<views::DecodedImageCache>(budgetKiB)),
      images_(*privateCache_), timeoutMs_(timeoutMs) {}

bool QtImageCache::contains(const QString& uri) const {
  Q_ASSERT(QThread::currentThread() == thread());
  return images_.contains(uri);
}

void QtImageCache::decode(const QString& uri, const QByteArray& bytes,
                          bool cache, Completion complete) {
  QBuffer buffer;
  buffer.setData(bytes);
  buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer);
  const QSize dimensions = reader.size();
  if (!dimensions.isValid() ||
      static_cast<qint64>(dimensions.width()) * dimensions.height() > kMaxPixels) {
    complete({}, QStringLiteral("Invalid image or image exceeds pixel limit"));
    return;
  }
  QImage image = reader.read();
  if (image.isNull()) {
    complete({}, QStringLiteral("Could not decode image"));
    return;
  }
  const auto size = image.size();
  if (cache) images_.insert(uri, std::move(image));
  complete(size, {});
}

void QtImageCache::load(const QString& uri, const Headers& headers,
                       Completion complete) {
  Q_ASSERT(QThread::currentThread() == thread());
  if (headers.empty()) {
    if (const auto image = images_.image(views::DecodedImageCache::key(uri)); !image.isNull()) {
      complete(image.size(), {});
      return;
    }
  }
  const QUrl url(uri);
  const auto scheme = url.scheme().toLower();
  if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")) {
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()) {
      complete({}, QStringLiteral("Invalid HTTP image URL"));
      return;
    }
    QNetworkRequest request(url);
    // Do not forward credentials or other caller headers across redirects.
    // Callers can explicitly retry the destination URL after reviewing it.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setTransferTimeout(timeoutMs_);
    for (const auto& [name, value] : headers) {
      if (name.isEmpty() || name.contains('\r') || name.contains('\n') ||
          name.contains(':') || value.contains('\r') || value.contains('\n')) {
        complete({}, QStringLiteral("Invalid image request header"));
        return;
      }
      request.setRawHeader(name, value);
    }
    auto* reply = network_.get(request);
    reply->setReadBufferSize(kMaxEncodedBytes + 1);
    auto bytes = std::make_shared<QByteArray>();
    auto failure = std::make_shared<QString>();
    auto* deadline = new QTimer(reply);
    deadline->setSingleShot(true);
    QObject::connect(deadline, &QTimer::timeout, reply, [reply, failure] {
      *failure = QStringLiteral("Image request timed out");
      reply->abort();
    });
    deadline->start(timeoutMs_);
    QObject::connect(reply, &QNetworkReply::readyRead, this,
                     [reply, bytes, failure] {
      bytes->append(reply->readAll());
      if (bytes->size() > kMaxEncodedBytes) {
        *failure = QStringLiteral("Image exceeds encoded byte limit");
        reply->abort();
      }
    });
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, bytes, failure, uri, cache = headers.empty(),
                      complete = std::move(complete)]() mutable {
      bytes->append(reply->readAll());
      const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (failure->isEmpty() && bytes->size() > kMaxEncodedBytes) {
        *failure = QStringLiteral("Image exceeds encoded byte limit");
      }
      if (failure->isEmpty() && (reply->error() != QNetworkReply::NoError ||
                                status < 200 || status >= 300)) {
        *failure = QStringLiteral("Image request failed (HTTP %1)").arg(status);
      }
      // Respect private/no-store: never expose these in the URI-only cache.
      const auto control = reply->rawHeader("Cache-Control").toLower();
      cache = cache && !control.contains("no-store") && !control.contains("private");
      reply->deleteLater();
      if (!failure->isEmpty()) {
        complete({}, *failure);
      } else {
        decode(uri, *bytes, cache, std::move(complete));
      }
    });
    return;
  }
  if (!headers.empty()) {
    complete({}, QStringLiteral("Headers require an HTTP(S) image URL"));
    return;
  }
  QByteArray bytes;
  if (scheme == QStringLiteral("data")) {
    const auto comma = uri.indexOf(',');
    if (comma < 0 || !uri.left(comma).endsWith(QStringLiteral(";base64"))) {
      complete({}, QStringLiteral("Only base64 image data URIs are supported"));
      return;
    }
    const auto payload = uri.mid(comma + 1).toLatin1();
    if (payload.size() > (kMaxEncodedBytes * 4 / 3 + 4)) {
      complete({}, QStringLiteral("Image exceeds encoded byte limit"));
      return;
    }
    bytes = QByteArray::fromBase64(payload, QByteArray::AbortOnBase64DecodingErrors);
  } else {
    QString path;
    if (scheme == QStringLiteral("file")) path = url.toLocalFile();
    else if (scheme == QStringLiteral("qrc")) path = QLatin1Char(':') + url.path();
    else if (uri.startsWith('/')) path = uri;
    else {
      complete({}, QStringLiteral("Unsupported image URI scheme"));
      return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxEncodedBytes) {
      complete({}, QStringLiteral("Could not read image or image exceeds byte limit"));
      return;
    }
    bytes = file.read(kMaxEncodedBytes + 1);
  }
  if (bytes.size() > kMaxEncodedBytes) {
    complete({}, QStringLiteral("Image exceeds encoded byte limit"));
    return;
  }
  decode(uri, bytes, true, std::move(complete));
}

}  // namespace rnqnx::runtime
