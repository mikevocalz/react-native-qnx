#include "react-native-qnx/runtime/MetroReloadSession.h"

#include <QMetaObject>
#include <QKeyEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QThread>
#include <QUrlQuery>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace rnqnx::runtime {

bool MetroReloadSession::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::KeyPress) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_F5 && !key->isAutoRepeat()) {
      requestReload(QStringLiteral("F5"));
      return true;
    }
  }
  return QObject::eventFilter(watched, event);
}

QString MetroReloadSession::validate(const Configuration& config) {
  const auto& url = config.bundleUrl;
  if (!url.isValid() || url.host().isEmpty() || url.hasFragment() ||
      !url.userInfo().isEmpty() ||
      (url.scheme() != "http" || url.host() != "127.0.0.1") ||
      !url.path().endsWith(".bundle")) {
    return QStringLiteral("Provide an explicit loopback HTTP Metro .bundle URL without credentials or fragment");
  }
  const QUrlQuery query(url);
  for (const auto& item : {std::pair{"dev", "true"},
                          {"platform", "qnx"}}) {
    if (query.allQueryItemValues(QString::fromLatin1(item.first)) !=
        QStringList{QString::fromLatin1(item.second)}) {
      return QStringLiteral("Metro URL requires exactly dev=true and platform=qnx");
    }
  }
  if (config.componentName.empty() || config.fetchTimeoutMs < 1 ||
      config.fetchTimeoutMs > 60000 || config.maxBundleBytes < 1 ||
      config.maxBundleBytes > 64 * 1024 * 1024 ||
      config.maxFetchAttempts < 1 || config.maxFetchAttempts > 5) {
    return QStringLiteral("Invalid development component, fetch deadline, byte limit or retry bound");
  }
  return {};
}

MetroReloadSession::MetroReloadSession(fabric::ShadowViewMutationAdapter& adapter,
    Configuration configuration, PrepareHost prepareHost)
    : adapter_(adapter), configuration_(std::move(configuration)),
      prepareHost_(std::move(prepareHost)) {
  const auto error = validate(configuration_);
  if (!error.isEmpty()) throw std::invalid_argument(error.toStdString());
  network_.setProxy(QNetworkProxy::NoProxy);
  fetchDeadline_.setSingleShot(true);
  fetchRetry_.setSingleShot(true);
  QObject::connect(&fetchDeadline_, &QTimer::timeout, this, [this] {
    fetchFailed(QStringLiteral("Metro initial fetch exceeded its deadline"));
  });
  QObject::connect(&fetchRetry_, &QTimer::timeout, this, [this] { fetch(); });
}

MetroReloadSession::~MetroReloadSession() {
  listeners_.clear();
  stop();
}

void MetroReloadSession::requireOwner() const {
  if (QThread::currentThread() != thread()) {
    throw std::logic_error("MetroReloadSession requires the owning Qt thread");
  }
}

MetroReloadSession::Status MetroReloadSession::status() const { requireOwner(); return status_; }

MetroReloadSession::Subscription MetroReloadSession::addStatusListener(
    std::function<void(const Status&)> listener) {
  requireOwner();
  const auto id = ++nextListener_;
  listeners_.emplace(id, listener);
  const auto self = QPointer<MetroReloadSession>(this);
  listener(status_);
  return {[self, id] {
    if (self) { self->requireOwner(); self->listeners_.erase(id); }
  }};
}

void MetroReloadSession::publish(Phase phase, QString message) {
  status_.phase = phase;
  status_.message = std::move(message);
  const auto snapshot = status_;
  QMetaObject::invokeMethod(this, [self = QPointer<MetroReloadSession>(this), snapshot] {
    if (!self || self->status_.generation != snapshot.generation) return;
    const auto listeners = self->listeners_;
    for (const auto& [id, listener] : listeners) {
      (void)id;
      listener(snapshot);
      if (!self || self->status_.generation != snapshot.generation) return;
    }
  }, Qt::QueuedConnection);
}

void MetroReloadSession::start() {
  requireOwner();
  if (status_.phase != Phase::Stopped) return;
  beginGeneration();
}

void MetroReloadSession::requestReload(const QString& reason) {
  requireOwner();
  if (status_.phase == Phase::Stopped || reloadPending_) return;
  (void)reason;
  reloadPending_ = true;
  const auto generation = status_.generation;
  const auto replace = [self = QPointer<MetroReloadSession>(this), generation] {
    if (self && self->status_.phase != Phase::Stopped &&
        self->status_.generation == generation) self->beginGeneration();
  };
  // Reload can be requested inside JS. Destruction must happen after that RN task,
  // including microtasks/rendering, not inside the native method it called.
  if (host_) host_->whenIdle(replace);
  else QMetaObject::invokeMethod(this, replace, Qt::QueuedConnection);
}

void MetroReloadSession::beginGeneration() {
  ++status_.generation;
  reloadPending_ = false;
  cancelFetch();
  replaceHost();
  fetchAttempts_ = 0;
  fetch();
}

void MetroReloadSession::cancelFetch() {
  fetchDeadline_.stop();
  fetchRetry_.stop();
  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    reply_->abort();
    reply_->deleteLater();
    reply_.clear();
  }
  source_.clear();
}

void MetroReloadSession::replaceHost() {
  if (host_) {
    host_.reset();
  }
  surfaceStarted_ = false;
  bundleReady_ = false;
}

void MetroReloadSession::stop() {
  requireOwner();
  if (status_.phase == Phase::Stopped) return;
  ++status_.generation;
  reloadPending_ = false;
  cancelFetch();
  replaceHost();
  publish(Phase::Stopped, {});
}

void MetroReloadSession::fetch() {
  ++fetchAttempts_;
  const auto generation = status_.generation;
  publish(Phase::Fetching, QStringLiteral("Fetching Metro development bundle"));
  QNetworkRequest request(configuration_.bundleUrl);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                       QNetworkRequest::AlwaysNetwork);
  request.setTransferTimeout(configuration_.fetchTimeoutMs);
  reply_ = network_.get(request);
  reply_->setReadBufferSize(configuration_.maxBundleBytes + 1);
  fetchDeadline_.start(configuration_.fetchTimeoutMs);
  auto* reply = reply_.data();
  const auto consume = [this, generation, reply] {
    if (generation != status_.generation || reply_ != reply) return false;
    source_.append(reply->read(configuration_.maxBundleBytes + 1 - source_.size()));
    if (source_.size() > configuration_.maxBundleBytes) {
      fetchFailed(QStringLiteral("Metro bundle exceeded the configured byte limit"));
      return false;
    }
    return true;
  };
  QObject::connect(reply, &QIODevice::readyRead, this, [consume] { consume(); });
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, consume] {
    if (!consume()) return;
    fetchDeadline_.stop();
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || code != 200 || source_.isEmpty()) {
      fetchFailed(QStringLiteral("Metro fetch failed (HTTP %1): %2\n%3")
          .arg(code).arg(reply->errorString(), QString::fromUtf8(source_.left(8192))));
      return;
    }
    const auto source = std::move(source_);
    reply_->deleteLater();
    reply_.clear();
    publish(Phase::Loading, QStringLiteral("Evaluating Metro bundle"));
    try {
      host_ = std::make_unique<QtReactHost>(adapter_);
      host_->installTurboModuleBinding();
      host_->installComponentProvider();
      host_->installPlatformConstants();
      host_->installTimers();
      if (prepareHost_) prepareHost_(*host_);
      const bool evaluated = host_->evaluate(source.toStdString(),
          configuration_.bundleUrl.toString(QUrl::FullyEncoded).toStdString());
      if (!evaluated) {
        publish(Phase::Error, host_->lastError());
        return; // Manual full reload after fixing the bundle.
      }
      bundleReady_ = true;
      startSurfaceIfReady();
    } catch (const std::exception& error) {
      publish(Phase::Error, QString::fromUtf8(error.what()));
    }
  });
}

void MetroReloadSession::fetchFailed(const QString& message) {
  cancelFetch();
  publish(Phase::Error, message);
  if (fetchAttempts_ < configuration_.maxFetchAttempts) {
    fetchRetry_.start(500 * fetchAttempts_);
  }
}

void MetroReloadSession::resize(float width, float height) {
  requireOwner();
  if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) return;
  width_ = width;
  height_ = height;
  startSurfaceIfReady();
}

void MetroReloadSession::startSurfaceIfReady() {
  if (!host_ || !bundleReady_ || width_ <= 0 || height_ <= 0) return;
  try {
    if (!surfaceStarted_) {
      host_->startSurface(configuration_.surfaceId, configuration_.componentName, width_, height_);
      surfaceStarted_ = true;
    } else if (!host_->resizeSurface(configuration_.surfaceId, width_, height_)) {
      publish(Phase::Error, host_->lastError());
      return;
    }
    if (!host_->lastError().isEmpty()) publish(Phase::Error, host_->lastError());
    else publish(Phase::Ready, QStringLiteral("Development bundle loaded; full reload resets state"));
  } catch (const std::exception& error) {
    publish(Phase::Error, QString::fromUtf8(error.what()));
  }
}
} // namespace rnqnx::runtime
