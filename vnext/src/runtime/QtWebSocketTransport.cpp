#include "react-native-qnx/runtime/QtWebSocketTransport.h"

#include <QNetworkRequest>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketHandshakeOptions>

namespace rnqnx::runtime {
namespace {
constexpr quint64 kMessageLimit = 16 * 1024 * 1024;
bool token(const QByteArray& value) {
  static const QRegularExpression expression(
      QRegularExpression::anchoredPattern(QStringLiteral("[!#$%&'*+.^_`|~0-9A-Za-z-]+")));
  return expression.match(QString::fromLatin1(value)).hasMatch();
}
} // namespace

QtWebSocketTransport::QtWebSocketTransport(Emit emit, int connectTimeoutMs,
                                         int closeTimeoutMs)
    : emit_(std::move(emit)), connectTimeoutMs_(connectTimeoutMs),
      closeTimeoutMs_(closeTimeoutMs) {}

QtWebSocketTransport::~QtWebSocketTransport() {
  // Disable callbacks before QObject destroys sockets. In particular, abort()
  // can synchronously emit disconnected().
  for (const auto& [id, entry] : sockets_) {
    (void)id;
    QObject::disconnect(entry.socket, nullptr, this, nullptr);
    entry.socket->abort();
  }
}

void QtWebSocketTransport::connectSocket(
    int id, const QUrl& url, const QStringList& protocols,
    const QMap<QByteArray, QByteArray>& headers) {
  Q_ASSERT(QThread::currentThread() == thread());
  if (sockets_.contains(id)) {
    fail(id, QStringLiteral("Socket ID is already in use"));
    return;
  }
  if (!url.isValid() || url.host().isEmpty() || url.hasFragment() ||
      !url.userInfo().isEmpty() ||
      (url.scheme() != QStringLiteral("ws") &&
       url.scheme() != QStringLiteral("wss"))) {
    fail(id, QStringLiteral("Expected a ws:// or wss:// URL without credentials or fragment"));
    return;
  }
  QStringList seen;
  for (const auto& protocol : protocols) {
    if (!token(protocol.toUtf8()) || seen.contains(protocol)) {
      fail(id, QStringLiteral("Invalid or duplicate WebSocket subprotocol"));
      return;
    }
    seen.append(protocol);
  }
  QNetworkRequest request(url);
  QString origin;
  for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
    const auto lower = it.key().toLower();
    if (!token(it.key()) || it.value().contains('\r') ||
        it.value().contains('\n') || it.value().contains('\0') ||
        lower == "host" || lower == "connection" || lower == "upgrade" ||
        lower.startsWith("sec-websocket-")) {
      fail(id, QStringLiteral("Invalid or reserved WebSocket header"));
      return;
    }
    if (lower == "origin") {
      origin = QString::fromUtf8(it.value());
    } else {
      request.setRawHeader(it.key(), it.value());
    }
  }
  auto* socket = new QWebSocket(origin, QWebSocketProtocol::VersionLatest, this);
  socket->setMaxAllowedIncomingFrameSize(kMessageLimit);
  socket->setMaxAllowedIncomingMessageSize(kMessageLimit);
  auto* timer = new QTimer(socket);
  timer->setSingleShot(true);
  sockets_.emplace(id, Socket{socket, timer});
  QObject::connect(timer, &QTimer::timeout, this, [this, id] {
    fail(id, QStringLiteral("WebSocket connection or close handshake timed out"));
  });
  QObject::connect(socket, &QWebSocket::connected, this, [this, id, socket, timer] {
    timer->stop();
    sockets_.at(id).opened = true;
    emit_(Event{Event::Kind::Open, id, {}, socket->subprotocol()});
  });
  QObject::connect(socket, &QWebSocket::textMessageReceived, this,
                   [this, id](const QString& message) {
    emit_(Event{Event::Kind::Message, id, message, {}});
  });
  QObject::connect(socket, &QWebSocket::binaryMessageReceived, this,
                   [this, id](const QByteArray& message) {
    emit_(Event{Event::Kind::Message, id,
                QString::fromLatin1(message.toBase64()), {}, true});
  });
  QObject::connect(socket, &QWebSocket::errorOccurred, this,
                   [this, id, socket](QAbstractSocket::SocketError) {
    fail(id, socket->errorString());
  });
  QObject::connect(socket, &QWebSocket::disconnected, this, [this, id, socket] {
    if (!sockets_.at(id).opened ||
        socket->error() != QAbstractSocket::UnknownSocketError) {
      fail(id, socket->errorString());
      return;
    }
    finish(id, Event{Event::Kind::Closed, id, socket->closeReason(), {}, false,
                    static_cast<int>(socket->closeCode())});
  });
  QWebSocketHandshakeOptions options;
  options.setSubprotocols(protocols);
  timer->start(connectTimeoutMs_);
  // TLS peer verification remains enabled. Never call ignoreSslErrors().
  socket->open(request, options);
}

void QtWebSocketTransport::finish(int id, Event event) {
  const auto found = sockets_.find(id);
  if (found != sockets_.end()) {
    auto* socket = found->second.socket;
    found->second.timer->stop();
    sockets_.erase(found);
    QObject::disconnect(socket, nullptr, this, nullptr);
    socket->abort();
    socket->deleteLater();
  }
  emit_(std::move(event));
}

void QtWebSocketTransport::fail(int id, const QString& message) {
  // RN's websocketFailed handler emits both JS error and JS close. Sending a
  // second websocketClosed event would duplicate that terminal notification.
  finish(id, Event{Event::Kind::Failed, id, message, {}});
}

QWebSocket* QtWebSocketTransport::writable(int id) {
  Q_ASSERT(QThread::currentThread() == thread());
  const auto found = sockets_.find(id);
  if (found == sockets_.end() || found->second.closing ||
      found->second.socket->state() != QAbstractSocket::ConnectedState) {
    fail(id, QStringLiteral("WebSocket is not open"));
    return nullptr;
  }
  return found->second.socket;
}

void QtWebSocketTransport::sendText(int id, const QString& message) {
  if (auto* socket = writable(id)) {
    if (static_cast<quint64>(message.toUtf8().size()) > kMessageLimit) {
      fail(id, QStringLiteral("WebSocket message exceeds 16 MiB"));
    } else if (socket->sendTextMessage(message) < 0) {
      fail(id, QStringLiteral("WebSocket text send failed"));
    }
  }
}

void QtWebSocketTransport::sendBinary(int id, const QByteArray& base64) {
  if (auto* socket = writable(id)) {
    if (static_cast<quint64>(base64.size()) > ((kMessageLimit + 2) / 3) * 4) {
      fail(id, QStringLiteral("WebSocket message exceeds 16 MiB"));
      return;
    }
    const auto decoded = QByteArray::fromBase64Encoding(
        base64, QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || static_cast<quint64>(decoded.decoded.size()) > kMessageLimit) {
      fail(id, QStringLiteral("Invalid or oversized base64 WebSocket message"));
    } else if (socket->sendBinaryMessage(decoded.decoded) < 0) {
      fail(id, QStringLiteral("WebSocket binary send failed"));
    }
  }
}

void QtWebSocketTransport::ping(int id) {
  if (auto* socket = writable(id)) {
    socket->ping();
  }
}

void QtWebSocketTransport::closeSocket(int id, int code, const QString& reason) {
  Q_ASSERT(QThread::currentThread() == thread());
  const auto found = sockets_.find(id);
  if (found == sockets_.end() || found->second.closing) {
    return; // React Native closes again after a terminal event.
  }
  if ((code != 1000 && (code < 3000 || code > 4999)) ||
      reason.toUtf8().size() > 123) {
    fail(id, QStringLiteral("Invalid WebSocket close code or reason"));
    return;
  }
  if (found->second.socket->state() != QAbstractSocket::ConnectedState) {
    finish(id, Event{Event::Kind::Closed, id, reason, {}, false, code});
    return;
  }
  found->second.closing = true;
  found->second.timer->start(closeTimeoutMs_);
  found->second.socket->close(static_cast<QWebSocketProtocol::CloseCode>(code), reason);
}

} // namespace rnqnx::runtime
