#include "react-native-qnx/runtime/QtImageCache.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlEngine>
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QThread>

#include <iostream>
#include <stdexcept>

using rnqnx::runtime::QtImageCache;
namespace {
void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
struct Result { QSize size; QString error; };
Result load(QtImageCache& cache, const QString& uri, const QtImageCache::Headers& headers = {}) {
  bool done = false;
  Result result;
  cache.load(uri, headers, [&](QSize size, QString error) {
    check(!done, "completion delivered twice");
    result = {size, error};
    done = true;
  });
  QElapsedTimer timer;
  timer.start();
  while (!done && timer.elapsed() < 5000) {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  check(done, "load did not complete");
  return result;
}
class Server : public QTcpServer {
 public:
  QByteArray png;
  int requests{0};
  Server() {
    QImage image(7, 5, QImage::Format_RGBA8888);
    image.fill(Qt::red);
    QBuffer output(&png);
    output.open(QIODevice::WriteOnly);
    check(image.save(&output, "PNG"), "PNG encode failed");
    check(listen(QHostAddress::LocalHost, 0), "local server listen failed");
    QObject::connect(this, &QTcpServer::newConnection, this, [this] {
      while (auto* socket = nextPendingConnection()) {
        auto input = std::make_shared<QByteArray>();
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, input] {
          input->append(socket->readAll());
          if (!input->contains("\r\n\r\n")) return;
          ++requests;
          const auto path = input->split(' ').at(1);
          if (path == "/slow") return;
          QByteArray status = "200 OK";
          QByteArray body = png;
          QByteArray extra;
          if (path == "/missing") { status = "404 Not Found"; body = "missing"; }
          if (path == "/broken") body = "not an image";
          if (path == "/truncated") body = png.left(30);
          if (path == "/private") extra = "Cache-Control: no-store\r\n";
          if (path == "/auth" && !input->contains("X-Image-Key: secret\r\n")) {
            status = "403 Forbidden"; body = "denied";
          }
          if (path == "/redirect") { status = "302 Found"; extra = "Location: /auth\r\n"; body.clear(); }
          if (path == "/huge") body = QByteArray(16 * 1024 * 1024 + 1, 'x');
          socket->write("HTTP/1.1 " + status + "\r\n" + extra +
                        "Content-Length: " + QByteArray::number(body.size()) +
                        "\r\nConnection: close\r\n\r\n" + body);
          socket->disconnectFromHost();
        });
      }
    });
  }
  QString url(const char* path) const {
    return QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(QLatin1String(path));
  }
};
}
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    Server server;
    QtImageCache cache;
    check(!cache.contains(server.url("/ok")), "fresh cache reports hit");
    auto first = load(cache, server.url("/ok"));
    check(first.error.isEmpty() && first.size == QSize(7, 5), "HTTP dimensions failed");
    check(cache.contains(server.url("/ok")), "successful load not cached");
    QQmlEngine engine;
    rnqnx::views::QtQuickItemFactory factory(engine);
    const auto source = factory.resolveImageSource(server.url("/ok"));
    check(source.startsWith("image://rnqnx/cache/"), "prefetch not wired into visual image source");
    auto* provider = static_cast<rnqnx::views::QnxImageProvider*>(engine.imageProvider("rnqnx"));
    QSize renderedSize;
    const auto rendered = provider->requestImage(source.mid(QString("image://rnqnx/").size()), &renderedSize, {});
    check(!rendered.isNull() && renderedSize == QSize(7, 5), "prefetched visual image missing");
    const auto before = server.requests;
    check(load(cache, server.url("/ok")).size == QSize(7, 5), "cache reuse failed");
    check(server.requests == before, "cache reuse made network request");
    for (auto path : {"/missing", "/broken", "/truncated", "/redirect", "/huge"}) {
      check(!load(cache, server.url(path)).error.isEmpty(), "failure accepted as success");
      check(!cache.contains(server.url(path)), "failure cached");
    }
    check(!load(cache, server.url("/auth")).error.isEmpty(), "auth missing not rejected");
    check(load(cache, server.url("/auth"), {{"X-Image-Key", "secret"}}).size == QSize(7, 5),
          "headers were not sent");
    check(!cache.contains(server.url("/auth")), "header-specific response leaked into public cache");
    check(!load(cache, server.url("/auth")).error.isEmpty(), "auth response reused without headers");
    check(!load(cache, server.url("/ok"), {{"Bad\nName", "value"}}).error.isEmpty(), "invalid header accepted");
    check(load(cache, server.url("/private")).size == QSize(7, 5), "no-store image not decoded");
    check(!cache.contains(server.url("/private")), "no-store cached");
    QtImageCache timeoutCache(32 * 1024, 25);
    check(!load(timeoutCache, server.url("/slow")).error.isEmpty(), "timeout did not reject");
    QtImageCache evictionCache(1);
    check(load(evictionCache, server.url("/one")).size.isValid(), "eviction first load");
    check(load(evictionCache, server.url("/two")).size.isValid(), "eviction second load");
    check(!evictionCache.contains(server.url("/one")) && evictionCache.contains(server.url("/two")),
          "bounded LRU cache failed");
    const auto data = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(server.png.toBase64());
    check(load(cache, data).size == QSize(7, 5) && cache.contains(data), "data URI failed");
    check(!load(cache, "data:image/png;base64,!!!!").error.isEmpty(), "invalid base64 accepted");
    check(!load(cache, "data:image/png,abc").error.isEmpty(), "unsupported data URI accepted");
    check(!load(cache, "ftp://example.invalid/image").error.isEmpty(), "unsupported scheme accepted");
    QTemporaryFile file;
    check(file.open(), "temporary file failed");
    file.write(server.png); file.flush();
    check(load(cache, file.fileName()).size == QSize(7, 5), "local file failed");
    check(!load(cache, "/nonexistent/image.png").error.isEmpty(), "missing file accepted");
    // Destroy the request owner while I/O is pending: no callback after teardown.
    bool called = false;
    { QtImageCache temporary; temporary.load(server.url("/slow"), {}, [&](QSize, QString) { called = true; }); }
    QCoreApplication::processEvents();
    check(!called, "callback escaped cache lifetime");
    std::cout << "RNQNX_IMAGE_CACHE_OK HTTP headers failures timeout limits LRU teardown local-data\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
