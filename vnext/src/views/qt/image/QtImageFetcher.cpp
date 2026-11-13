#include "react-native-qnx/views/image/QtImageFetcher.h"

#include "react-native-qnx/views/image/ImageDiskCache.h"

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace rnqnx::views {

ImageCachePolicy parseCachePolicy(const QString& text) {
  if (text == QLatin1String("none")) return ImageCachePolicy::None;
  if (text == QLatin1String("disk")) return ImageCachePolicy::Disk;
  if (text == QLatin1String("memory")) return ImageCachePolicy::Memory;
  return ImageCachePolicy::MemoryDisk;
}

QString ImageRequest::storageKey() const {
  if (!cacheKey.isEmpty()) return QStringLiteral("key:") + cacheKey;
  if (headers.empty()) return uri;
  QByteArray folded = uri.toUtf8();
  for (const auto& [name, value] : headers) {  // std::map: already sorted
    folded.append('\n').append(name.toLower()).append(':').append(value);
  }
  return uri + QLatin1Char('#') +
         QString::fromLatin1(QCryptographicHash::hash(folded, QCryptographicHash::Sha256).toHex());
}

struct QtImageFetcher::Pending {
  QPointer<QNetworkReply> reply;
  ImageFetchQueue::Id queued{0};
  bool started{false};
  QByteArray bytes;
  QString failure;
  ImageRequest request;
  Progress progress;
  Done done;
};

QtImageFetcher::QtImageFetcher(int timeoutMs, ImageDiskCache* disk, ImageFetchQueue* queue, int maxConcurrent)
    : disk_(disk), timeoutMs_(timeoutMs), ownQueue_(maxConcurrent), queue_(queue != nullptr ? queue : &ownQueue_) {}

FetchResult QtImageFetcher::readLocal(const QString& uri) const {
  FetchResult result;
  result.origin = ImageOrigin::Local;
  const QUrl url(uri);
  const QString scheme = url.scheme().toLower();
  QByteArray bytes;
  if (scheme == QLatin1String("data")) {
    const auto comma = uri.indexOf(QLatin1Char(','));
    if (comma < 0 || !uri.left(comma).endsWith(QLatin1String(";base64"))) {
      result.error = QStringLiteral("Only base64 image data URIs are supported");
      return result;
    }
    const QByteArray payload = uri.mid(comma + 1).toLatin1();
    if (payload.size() > (kMaxEncodedBytes * 4 / 3 + 4)) {
      result.error = QStringLiteral("Image exceeds encoded byte limit");
      return result;
    }
    bytes = QByteArray::fromBase64(payload, QByteArray::AbortOnBase64DecodingErrors);
  } else {
    QString path;
    if (scheme == QLatin1String("file")) path = url.toLocalFile();
    else if (scheme == QLatin1String("qrc")) path = QLatin1Char(':') + url.path();
    else if (uri.startsWith(QLatin1Char('/'))) path = uri;
    else {
      result.error = QStringLiteral("Unsupported image URI scheme");
      return result;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxEncodedBytes) {
      result.error = QStringLiteral("Could not read image or image exceeds byte limit");
      return result;
    }
    bytes = file.read(kMaxEncodedBytes + 1);
  }
  if (bytes.size() > kMaxEncodedBytes) {
    result.error = QStringLiteral("Image exceeds encoded byte limit");
    return result;
  }
  result.bytes = std::move(bytes);
  return result;
}

QtImageFetcher::Id QtImageFetcher::fetch(const ImageRequest& request, Progress progress, Done done) {
  const Id id = nextId_++;
  const QUrl url(request.uri);
  const QString scheme = url.scheme().toLower();
  const bool http = scheme == QLatin1String("http") || scheme == QLatin1String("https");

  if (!http) {
    FetchResult result;
    if (!request.headers.empty()) {
      result.error = QStringLiteral("Headers require an HTTP(S) image URL");
    } else {
      result = readLocal(request.uri);
    }
    // Local sources complete before fetch returns, like a cache hit.
    done(std::move(result));
    return id;
  }

  if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()) {
    FetchResult result;
    result.error = QStringLiteral("Invalid HTTP image URL");
    done(std::move(result));
    return id;
  }
  if (disk_ != nullptr && usesDisk(request.policy)) {
    QByteArray cached = disk_->read(request.storageKey());
    if (!cached.isEmpty()) {
      FetchResult result;
      result.bytes = std::move(cached);
      result.origin = ImageOrigin::Disk;
      done(std::move(result));
      return id;
    }
  }

  for (const auto& [name, value] : request.headers) {
    if (name.isEmpty() || name.contains('\r') || name.contains('\n') || name.contains(':') ||
        value.contains('\r') || value.contains('\n')) {
      FetchResult result;
      result.error = QStringLiteral("Invalid image request header");
      done(std::move(result));
      return id;
    }
  }

  auto pending = std::make_shared<Pending>(Pending{{}, 0, false, {}, {}, request, std::move(progress), std::move(done)});
  pending_.emplace(id, pending);
  // The queue may start the load before submit returns.
  const auto queued = queue_->submit(request.priority, [this, id] { startHttp(id); });
  if (!pending->started) pending->queued = queued;
  return id;
}

void QtImageFetcher::startHttp(Id id) {
  const auto found = pending_.find(id);
  if (found == pending_.end()) {
    queue_->release();
    return;
  }
  const std::shared_ptr<Pending> pending = found->second;
  pending->started = true;
  ++started_;
  QNetworkRequest net{QUrl(pending->request.uri)};
  // Do not forward credentials or other caller headers across redirects.
  net.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  net.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
  net.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
  net.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
  net.setTransferTimeout(timeoutMs_);
  for (const auto& [name, value] : pending->request.headers) net.setRawHeader(name, value);

  QNetworkReply* reply = network_.get(net);
  reply->setReadBufferSize(kMaxEncodedBytes + 1);
  pending->reply = reply;

  auto* deadline = new QTimer(reply);
  deadline->setSingleShot(true);
  QObject::connect(deadline, &QTimer::timeout, reply, [pending, reply = pending->reply.data()] {
    pending->failure = QStringLiteral("Image request timed out");
    reply->abort();
  });
  deadline->start(timeoutMs_);
  QObject::connect(reply, &QNetworkReply::readyRead, this, [pending, reply = pending->reply.data()] {
    pending->bytes.append(reply->readAll());
    if (pending->bytes.size() > kMaxEncodedBytes) {
      pending->failure = QStringLiteral("Image exceeds encoded byte limit");
      reply->abort();
    }
  });
  QObject::connect(reply, &QNetworkReply::downloadProgress, this,
                   [pending](qint64 received, qint64 total) {
                     if (pending->progress) pending->progress(received, total);
                   });
  QObject::connect(reply, &QNetworkReply::finished, this, [this, id] {
    const auto done = pending_.find(id);
    if (done == pending_.end()) return;
    const std::shared_ptr<Pending> owned = done->second;
    pending_.erase(done);
    --started_;
    // Free the slot first so the next waiting load starts while this one's
    // callback runs.
    queue_->release();
    finishHttp(id, *owned);
  });
}

void QtImageFetcher::finishHttp(Id, Pending& pending) {
  QNetworkReply* reply = pending.reply;
  pending.bytes.append(reply->readAll());
  const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (pending.failure.isEmpty() && pending.bytes.size() > kMaxEncodedBytes) {
    pending.failure = QStringLiteral("Image exceeds encoded byte limit");
  }
  if (pending.failure.isEmpty() &&
      (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300)) {
    // No HTTP status means the request never got an answer: name the network
    // reason (DNS, refused, TLS verification) instead of "HTTP 0".
    pending.failure = status == 0
        ? QStringLiteral("Image request failed: %1").arg(reply->errorString())
        : QStringLiteral("Image request failed (HTTP %1)").arg(status);
  }
  const QByteArray control = reply->rawHeader("Cache-Control").toLower();
  reply->deleteLater();

  FetchResult result;
  result.origin = ImageOrigin::Network;
  result.cacheable = !control.contains("no-store") && !control.contains("private");
  if (!pending.failure.isEmpty()) {
    result.error = pending.failure;
  } else {
    result.bytes = std::move(pending.bytes);
    if (disk_ != nullptr && usesDisk(pending.request.policy) && result.cacheable) {
      disk_->write(pending.request.storageKey(), result.bytes);
    }
  }
  pending.done(std::move(result));
}

void QtImageFetcher::cancel(Id id) {
  const auto found = pending_.find(id);
  if (found == pending_.end()) return;
  const std::shared_ptr<Pending> owned = found->second;
  pending_.erase(found);
  if (!owned->started) {
    queue_->cancel(owned->queued);
    return;
  }
  --started_;
  if (owned->reply) {
    // Drop every connection (the timer and read callbacks hold `owned`), then
    // abort: no callback can run after cancel returns.
    owned->reply->disconnect();
    owned->reply->abort();
    owned->reply->deleteLater();
  }
  queue_->release();
}

QtImageFetcher::~QtImageFetcher() {
  // Waiting loads first, so freeing a running load's slot starts nothing.
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (it->second->started) {
      ++it;
    } else {
      queue_->cancel(it->second->queued);
      it = pending_.erase(it);
    }
  }
  while (!pending_.empty()) cancel(pending_.begin()->first);
}

}  // namespace rnqnx::views
