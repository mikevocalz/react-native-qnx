#include "react-native-qnx/views/image/QtImageFetcher.h"

#include "react-native-qnx/views/image/ImageDiskCache.h"
#include "react-native-qnx/views/image/ImageDecode.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QPromise>
#include <QThread>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <atomic>

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

namespace {
QNetworkAccessManager* ownerNetwork() {
  thread_local QPointer<QNetworkAccessManager> manager;
  if (!manager) {
    auto* app = QCoreApplication::instance();
    manager = new QNetworkAccessManager(app && app->thread() == QThread::currentThread() ? app : nullptr);
    if (!manager->parent())
      QObject::connect(QThread::currentThread(), &QThread::finished, manager, &QObject::deleteLater);
  }
  return manager;
}
}

struct QtImageFetcher::Pending {
  QPointer<QNetworkReply> reply;
  ImageFetchQueue::Id queued{0};
  bool started{false};
  QByteArray bytes;
  QString failure;
  ImageRequest request;
  QUrl currentUrl;
  ImageHeaders forwardedHeaders;
  int redirects{0};
  bool cacheable{true};
  QElapsedTimer elapsed;
  std::shared_ptr<std::atomic_bool> cancelled{std::make_shared<std::atomic_bool>(false)};
  Progress progress;
  Done done;
};

QtImageFetcher::QtImageFetcher(int timeoutMs, ImageDiskCache* disk, ImageFetchQueue* queue,
                               int maxConcurrent, QThreadPool* pool)
    : network_(ownerNetwork()), disk_(disk ? std::make_shared<ImageDiskCache>(*disk) : nullptr),
      pool_(pool ? pool : QThreadPool::globalInstance()), timeoutMs_(timeoutMs),
      ownQueue_(maxConcurrent), queue_(queue != nullptr ? queue : &ownQueue_) {}

FetchResult QtImageFetcher::readLocal(const QString& uri) {
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

  if ((http && (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty())) ||
      (!http && !request.headers.empty())) {
    FetchResult result;
    result.error = QStringLiteral("Invalid image URL or non-HTTP headers");
    done(std::move(result));
    return id;
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

  auto pending = std::make_shared<Pending>();
  pending->request = request;
  pending->currentUrl = url;
  pending->forwardedHeaders = request.headers;
  pending->progress = std::move(progress);
  pending->done = std::move(done);
  pending_.emplace(id, pending);
  const auto queued = queue_->submit(request.priority, [this, id] { startLoad(id); });
  if (!pending->started) pending->queued = queued;
  return id;
}

void QtImageFetcher::work(Id id, std::function<FetchResult()> task, bool networkOnMiss) {
  const auto found = pending_.find(id);
  if (found == pending_.end()) return;
  auto cancelled = found->second->cancelled;
  auto promise = std::make_shared<QPromise<FetchResult>>();
  promise->start();
  auto* watcher = new QFutureWatcher<FetchResult>(this);
  QObject::connect(watcher, &QFutureWatcher<FetchResult>::finished, this,
                   [this, id, watcher, networkOnMiss] {
    FetchResult result = watcher->result();
    watcher->deleteLater();
    if (pending_.find(id) == pending_.end()) return;
    if (networkOnMiss && result.bytes.isEmpty() && result.error.isEmpty()) startHttp(id);
    else complete(id, std::move(result));
  });
  watcher->setFuture(promise->future());
  // Worker captures contain no QObject, JSI value, or owner-thread completion.
  pool_->start([promise, cancelled, task = std::move(task)] {
    FetchResult result;
    if (!cancelled->load()) result = task();
    promise->addResult(std::move(result));
    promise->finish();
  });
}

void QtImageFetcher::startLoad(Id id) {
  const auto found = pending_.find(id);
  if (found == pending_.end()) { queue_->release(); return; }
  auto pending = found->second;
  pending->started = true;
  pending->elapsed.start();
  ++started_;
  const QString scheme = pending->currentUrl.scheme().toLower();
  if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
    work(id, [uri = pending->request.uri] { return readLocal(uri); });
  } else if (disk_ && usesDisk(pending->request.policy)) {
    work(id, [disk = disk_, key = pending->request.storageKey()] {
      FetchResult result;
      result.origin = ImageOrigin::Disk;
      result.bytes = disk->read(key, kMaxEncodedBytes);
      if (!result.bytes.isEmpty() &&
          !decodeImage(result.bytes, QSize(1, 1), true, ContentFit::Cover).error.isEmpty()) {
        disk->remove(key);
        result.bytes.clear();
      }
      return result;
    }, true);
  } else startHttp(id);
}

void QtImageFetcher::complete(Id id, FetchResult result) {
  const auto found = pending_.find(id);
  if (found == pending_.end()) return;
  auto owned = found->second;
  pending_.erase(found);
  --started_;
  queue_->release();
  owned->done(std::move(result));
}

void QtImageFetcher::startHttp(Id id) {
  const auto found = pending_.find(id);
  if (found == pending_.end()) {
    queue_->release();
    return;
  }
  const std::shared_ptr<Pending> pending = found->second;
  const int remaining = timeoutMs_ - static_cast<int>(pending->elapsed.elapsed());
  if (remaining <= 0) {
    FetchResult result;
    result.error = QStringLiteral("Image request timed out");
    complete(id, std::move(result));
    return;
  }
  QNetworkRequest net{pending->currentUrl};
  // Do not forward credentials or other caller headers across redirects.
  net.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  net.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
  net.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
  net.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
  net.setTransferTimeout(remaining);
  for (const auto& [name, value] : pending->forwardedHeaders) net.setRawHeader(name, value);

  QNetworkReply* reply = network_->get(net);
  reply->setReadBufferSize(kMaxEncodedBytes + 1);
  pending->reply = reply;

  auto* deadline = new QTimer(reply);
  deadline->setSingleShot(true);
  QObject::connect(deadline, &QTimer::timeout, this, [this, id] {
    const auto found = pending_.find(id);
    if (found == pending_.end() || !found->second->reply) return;
    found->second->failure = QStringLiteral("Image request timed out");
    found->second->reply->abort();
  });
  deadline->start(remaining);
  QObject::connect(reply, &QNetworkReply::readyRead, this, [this, id] {
    const auto found = pending_.find(id);
    if (found == pending_.end() || !found->second->reply) return;
    auto& pending = *found->second;
    if (pending.reply->isOpen()) pending.bytes.append(pending.reply->readAll());
    if (pending.bytes.size() > kMaxEncodedBytes) {
      pending.failure = QStringLiteral("Image exceeds encoded byte limit");
      pending.reply->abort();
    }
  });
  QObject::connect(reply, &QNetworkReply::downloadProgress, this,
                   [this, id](qint64 received, qint64 total) {
    const auto found = pending_.find(id);
    if (found != pending_.end() && found->second->progress) {
      auto progress = found->second->progress;
      progress(received, total);
    }
  });
  QObject::connect(reply, &QNetworkReply::finished, this, [this, id] { finishHttp(id); });
}

void QtImageFetcher::finishHttp(Id id) {
  const auto found = pending_.find(id);
  if (found == pending_.end()) return;
  const auto owned = found->second;
  auto& pending = *owned;
  QNetworkReply* reply = pending.reply;
  if (reply->isOpen()) pending.bytes.append(reply->readAll());
  const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QByteArray control = reply->rawHeader("Cache-Control").toLower();
  pending.cacheable = pending.cacheable && !control.contains("no-store") && !control.contains("private");
  const QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
  reply->disconnect(this);
  reply->deleteLater();
  pending.reply = nullptr;
  if (pending.failure.isEmpty() && pending.bytes.size() > kMaxEncodedBytes)
    pending.failure = QStringLiteral("Image exceeds encoded byte limit");

  if (pending.failure.isEmpty() && status >= 300 && status < 400 && !redirect.isEmpty()) {
    const QUrl next = pending.currentUrl.resolved(redirect);
    const QString scheme = next.scheme().toLower();
    if (++pending.redirects > 5 || !next.isValid() || next.host().isEmpty() ||
        !next.userInfo().isEmpty() ||
        (scheme != QLatin1String("https") && scheme != QLatin1String("http")) ||
        (pending.currentUrl.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0 &&
         scheme == QLatin1String("http"))) {
      pending.failure = QStringLiteral("Unsafe image redirect or redirect limit exceeded");
    } else {
      if (next.scheme().compare(pending.currentUrl.scheme(), Qt::CaseInsensitive) != 0 ||
          next.host().compare(pending.currentUrl.host(), Qt::CaseInsensitive) != 0 ||
          next.port(scheme == QLatin1String("https") ? 443 : 80) !=
              pending.currentUrl.port(pending.currentUrl.scheme() == QLatin1String("https") ? 443 : 80))
        pending.forwardedHeaders.clear();
      pending.currentUrl = next;
      pending.bytes.clear();
      startHttp(id);
      return;
    }
  }
  if (pending.failure.isEmpty() &&
      (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300)) {
    pending.failure = status == 0
        ? QStringLiteral("Image request failed: %1").arg(reply->errorString())
        : QStringLiteral("Image request failed (HTTP %1)").arg(status);
  }
  FetchResult result;
  result.origin = ImageOrigin::Network;
  result.cacheable = pending.cacheable;
  result.error = pending.failure;
  if (result.error.isEmpty()) result.bytes = std::move(pending.bytes);
  if (result.error.isEmpty() && disk_ && usesDisk(pending.request.policy) && result.cacheable) {
    work(id, [disk = disk_, key = pending.request.storageKey(), result = std::move(result)] {
      disk->write(key, result.bytes);
      return result;
    });
  } else complete(id, std::move(result));
}

void QtImageFetcher::cancel(Id id) {
  const auto found = pending_.find(id);
  if (found == pending_.end()) return;
  const std::shared_ptr<Pending> owned = found->second;
  pending_.erase(found);
  owned->cancelled->store(true);
  if (!owned->started) {
    queue_->cancel(owned->queued);
    return;
  }
  --started_;
  if (owned->reply) {
    // The pending callbacks are released on the owner thread. Workers retain
    // only a cancellation token and plain I/O values.
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
      it->second->cancelled->store(true);
      queue_->cancel(it->second->queued);
      it = pending_.erase(it);
    }
  }
  while (!pending_.empty()) cancel(pending_.begin()->first);
}

}  // namespace rnqnx::views
