#include "react-native-qnx/runtime/QtWebSocketTransport.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QFile>
#include <QProcess>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

using Transport = rnqnx::runtime::QtWebSocketTransport;
using Event = Transport::Event;
using Kind = Event::Kind;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

template <typename F>
void until(F condition, const char* message) {
  QElapsedTimer timer;
  timer.start();
  while (!condition() && timer.elapsed() < 3000) {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  check(condition(), message);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QWebSocketServer server(QStringLiteral("loopback"), QWebSocketServer::NonSecureMode);
  server.setSupportedSubprotocols({QStringLiteral("rnqnx-test")});
  check(server.listen(QHostAddress::LocalHost, 0), "listen");
  const QUrl url(QStringLiteral("ws://127.0.0.1:%1/echo").arg(server.serverPort()));
  std::vector<QWebSocket*> peers;
  bool headerSeen = false;
  QObject::connect(&server, &QWebSocketServer::newConnection, &app, [&] {
    auto* peer = server.nextPendingConnection();
    peer->setParent(&server);
    peers.push_back(peer);
    headerSeen = peer->request().rawHeader("x-rnqnx-test") == "loopback" &&
                 peer->origin() == QStringLiteral("https://example.test");
    QObject::connect(peer, &QWebSocket::textMessageReceived, peer,
                     [peer](const QString& text) { peer->sendTextMessage(text); });
    QObject::connect(peer, &QWebSocket::binaryMessageReceived, peer,
                     [peer](const QByteArray& data) { peer->sendBinaryMessage(data); });
  });
  std::vector<Event> events;
  auto transport = std::make_unique<Transport>([&](Event event) {
    events.push_back(std::move(event));
  }, 100, 100);
  const auto count = [&](int id, Kind kind) {
    return std::count_if(events.begin(), events.end(), [&](const Event& event) {
      return event.id == id && event.kind == kind;
    });
  };
  const auto connect = [&](int id) {
    const auto before = count(id, Kind::Open);
    transport->connectSocket(id, url, {QStringLiteral("rnqnx-test")},
        {{"x-rnqnx-test", "loopback"}, {"Origin", "https://example.test"}});
    until([&] { return count(id, Kind::Open) == before + 1; }, "open");
  };
  connect(1);
  check(headerSeen, "headers and Origin reach server");
  check(events.back().protocol == QStringLiteral("rnqnx-test"), "negotiated protocol");
  transport->sendText(1, QString::fromUtf8("hello \xF0\x9F\x8C\x8D"));
  transport->sendBinary(1, QByteArray("\0\xff\x7f", 3).toBase64());
  transport->ping(1);
  until([&] { return count(1, Kind::Message) == 2; }, "text and binary echoes");
  check(events[1].data == QString::fromUtf8("hello \xF0\x9F\x8C\x8D") &&
        !events[1].binary, "unicode text payload");
  check(events[2].data == QStringLiteral("AP9/") && events[2].binary,
        "base64 binary payload preserves NUL and non-UTF8 bytes");
  transport->closeSocket(1, 3001, QStringLiteral("bye"));
  transport->closeSocket(1, 3001, QStringLiteral("bye"));
  until([&] { return count(1, Kind::Closed) == 1; }, "local close");
  check(events.back().code == 3001 && events.back().data == QStringLiteral("bye"),
        "close code and reason");
  transport->closeSocket(1, 1000, {});
  check(transport->socketCount() == 0 && count(1, Kind::Failed) == 0,
        "close cleanup and idempotence");

  connect(2);
  peers.back()->close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("server bye"));
  until([&] { return count(2, Kind::Closed) == 1; }, "remote close");
  check(events.back().data == QStringLiteral("server bye"), "remote close reason");

  connect(3);
  transport->sendBinary(3, "%%%invalid");
  until([&] { return count(3, Kind::Failed) == 1; }, "invalid base64 fails");
  check(count(3, Kind::Closed) == 0 && transport->socketCount() == 0,
        "failure is terminal exactly once");

  connect(4);
  transport->connectSocket(4, url, {}, {});
  check(count(4, Kind::Failed) == 1 && transport->socketCount() == 0,
        "duplicate ID terminates original without opening replacement");
  connect(4); // IDs can be reused after cleanup.
  transport->closeSocket(4, 1006, {});
  check(count(4, Kind::Failed) == 2, "reserved close code rejected");

  transport->connectSocket(5, QUrl(QStringLiteral("http://localhost")), {}, {});
  transport->connectSocket(6, url, {QStringLiteral("bad protocol")}, {});
  transport->connectSocket(7, url, {}, {{"x-header", "bad\r\nvalue"}});
  transport->connectSocket(8, url, {}, {{"Sec-WebSocket-Key", "injected"}});
  for (int id = 5; id <= 8; ++id) {
    check(count(id, Kind::Failed) == 1, "invalid handshake rejected");
  }
  transport->connectSocket(22, url, {QStringLiteral("chat\n")}, {});
  transport->connectSocket(23, url, {}, {{"X-Test\n", "value"}});
  check(count(22, Kind::Failed) == 1 && count(23, Kind::Failed) == 1,
        "terminal newlines rejected in protocols and header names");
  transport->sendText(9, QStringLiteral("not connected"));
  check(count(9, Kind::Failed) == 1, "unknown socket fails");

  QTcpServer silentServer;
  check(silentServer.listen(QHostAddress::LocalHost, 0), "silent server listen");
  const QUrl silentUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(silentServer.serverPort()));
  transport->connectSocket(10, silentUrl, {}, {});
  until([&] { return count(10, Kind::Failed) == 1; }, "handshake timeout");
  check(count(10, Kind::Closed) == 0, "timeout emits no duplicate close");
  transport->connectSocket(11, silentUrl, {}, {});
  transport->closeSocket(11, 1000, QStringLiteral("cancelled"));
  check(count(11, Kind::Closed) == 1 && count(11, Kind::Failed) == 0,
        "close during connect cancels cleanly");

  QTcpServer temporary;
  check(temporary.listen(QHostAddress::LocalHost, 0), "temporary listen");
  const auto unusedPort = temporary.serverPort();
  temporary.close();
  transport->connectSocket(12,
      QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(unusedPort)), {}, {});
  until([&] { return count(12, Kind::Failed) == 1; }, "connection refusal");
  check(count(12, Kind::Closed) == 0, "refusal emits one terminal event");

  // Two live sockets keep their events isolated by ID.
  connect(15);
  connect(16);
  transport->sendText(15, QStringLiteral("first"));
  transport->sendText(16, QStringLiteral("second"));
  until([&] { return count(15, Kind::Message) == 1 &&
                    count(16, Kind::Message) == 1; }, "concurrent sockets");
  transport->closeSocket(15, 1000, {});
  transport->closeSocket(16, 1000, {});
  until([&] { return count(15, Kind::Closed) == 1 &&
                    count(16, Kind::Closed) == 1; }, "concurrent close");

  // Generate an ephemeral, deliberately untrusted localhost certificate. No
  // fixture credentials are persisted and no TLS validation is disabled.
  check(QSslSocket::supportsSsl(), "TLS backend available");
  QTemporaryDir certDir;
  check(certDir.isValid(), "temporary certificate directory");
  const auto certPath = certDir.filePath(QStringLiteral("cert.pem"));
  const auto keyPath = certDir.filePath(QStringLiteral("key.pem"));
  QProcess openssl;
  openssl.start(QStringLiteral("openssl"),
      {QStringLiteral("req"), QStringLiteral("-x509"), QStringLiteral("-newkey"),
       QStringLiteral("rsa:2048"), QStringLiteral("-nodes"),
       QStringLiteral("-keyout"), keyPath, QStringLiteral("-out"), certPath,
       QStringLiteral("-days"), QStringLiteral("1"), QStringLiteral("-subj"),
       QStringLiteral("/CN=localhost")});
  check(openssl.waitForFinished(10000) && openssl.exitCode() == 0,
        "generate ephemeral test certificate with openssl");
  QFile certFile(certPath);
  QFile keyFile(keyPath);
  check(certFile.open(QIODevice::ReadOnly) && keyFile.open(QIODevice::ReadOnly),
        "read ephemeral certificate");
  auto ssl = QSslConfiguration::defaultConfiguration();
  ssl.setLocalCertificate(QSslCertificate(certFile.readAll()));
  ssl.setPrivateKey(QSslKey(keyFile.readAll(), QSsl::Rsa));
  ssl.setPeerVerifyMode(QSslSocket::VerifyNone); // Server requires no client cert.
  QWebSocketServer tlsServer(QStringLiteral("untrusted"), QWebSocketServer::SecureMode);
  tlsServer.setSslConfiguration(ssl);
  check(tlsServer.listen(QHostAddress::LocalHost, 0), "TLS server listen");
  transport->connectSocket(17,
      QUrl(QStringLiteral("wss://localhost:%1/").arg(tlsServer.serverPort())), {}, {});
  until([&] { return count(17, Kind::Failed) == 1; }, "untrusted TLS fails closed");
  check(count(17, Kind::Open) == 0 && count(17, Kind::Closed) == 0,
        "untrusted TLS never opens or duplicates close");
  check(!events.back().data.contains(QStringLiteral("timed out")),
        "TLS rejected by handshake rather than timeout");

  connect(18);
  transport->closeSocket(18, 1000, QString(124, QLatin1Char('x')));
  check(count(18, Kind::Failed) == 1, "oversized close reason rejected");
  connect(19);
  peers.back()->abort();
  until([&] { return count(19, Kind::Failed) == 1; }, "abrupt peer loss fails");
  check(count(19, Kind::Closed) == 0, "abrupt peer loss is terminal once");

  // Minimal RFC6455 peer accepts the handshake, observes ping/close frames,
  // and deliberately never acknowledges close so the closing deadline is real.
  QTcpServer rawServer;
  check(rawServer.listen(QHostAddress::LocalHost, 0), "raw server listen");
  bool pingSeen = false;
  bool closeSeen = false;
  QObject::connect(&rawServer, &QTcpServer::newConnection, &app, [&] {
    auto* peer = rawServer.nextPendingConnection();
    peer->setParent(&rawServer);
    auto buffer = std::make_shared<QByteArray>();
    auto upgraded = std::make_shared<bool>(false);
    QObject::connect(peer, &QTcpSocket::readyRead, peer,
        [&, peer, buffer, upgraded] {
      buffer->append(peer->readAll());
      if (!*upgraded) {
        const auto end = buffer->indexOf("\r\n\r\n");
        if (end < 0) return;
        QByteArray key;
        for (const auto& line : buffer->left(end).split('\n')) {
          if (line.toLower().startsWith("sec-websocket-key:")) {
            key = line.mid(line.indexOf(':') + 1).trimmed();
          }
        }
        check(!key.isEmpty(), "raw handshake key");
        const auto accept = QCryptographicHash::hash(
            key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
            QCryptographicHash::Sha1).toBase64();
        peer->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                    "Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n");
        buffer->remove(0, end + 4);
        *upgraded = true;
      }
      while (buffer->size() >= 2) {
        const auto length = static_cast<unsigned char>((*buffer)[1]) & 0x7f;
        check(length < 126, "control frame length");
        const bool masked = (static_cast<unsigned char>((*buffer)[1]) & 0x80) != 0;
        const auto frameSize = 2 + (masked ? 4 : 0) + length;
        if (buffer->size() < frameSize) return;
        const auto opcode = static_cast<unsigned char>((*buffer)[0]) & 0x0f;
        if (opcode == 9) pingSeen = true;
        if (opcode == 8) closeSeen = true;
        buffer->remove(0, frameSize);
      }
    });
  });
  transport->connectSocket(20,
      QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(rawServer.serverPort())), {}, {});
  until([&] { return count(20, Kind::Open) == 1; }, "raw peer open");
  transport->ping(20);
  until([&] { return pingSeen; }, "ping reaches peer as control frame");
  transport->closeSocket(20, 1000, QStringLiteral("timeout"));
  until([&] { return count(20, Kind::Failed) == 1; }, "close deadline");
  check(closeSeen && count(20, Kind::Closed) == 0, "close sent with one timeout event");

  connect(21);
  transport->sendText(21, QString(16 * 1024 * 1024 + 1, QLatin1Char('x')));
  check(count(21, Kind::Failed) == 1, "outgoing message bound");

  connect(13);
  transport->sendText(13, QStringLiteral("pending at destruction"));
  transport->connectSocket(14, silentUrl, {}, {});
  const auto beforeDestruction = events.size();
  transport.reset();
  QElapsedTimer drain;
  drain.start();
  while (drain.elapsed() < 150) {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  check(events.size() == beforeDestruction, "no callbacks after destruction");
  std::cout << "RNQNX_WEBSOCKET_HOST_OK loopback headers protocol text binary ping close errors lifecycle\n";
  return 0;
}
