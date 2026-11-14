// The Image element's real load path: the host's QtImageCache over a local
// HTTP server, through QtRnsvgImageProvider, ImageLoader and an element's
// binding. No Fabric and no JavaScript.
//
// The server is a QTcpServer on 127.0.0.1, the technique tests/image-loader
// uses. A "gate" path holds its response until the test releases it, which
// makes in-flight cancellation and teardown deterministic without sleeping.
#include "RnsvgItems.h"
#include "image/ImageBinding.h"
#include "image/ImageDecoding.h"
#include "image/ImageElementProps.h"
#include "react-native-qnx/fabric/QtRnsvgImageProvider.h"
#include "react-native-qnx/runtime/QtImageCache.h"

#include <QBuffer>
#include <QDir>
#include <QGuiApplication>
#include <QHostAddress>
#include <QImage>
#include <QPainter>
#include <QPointer>
#include <QQuickItem>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include <map>
#include <memory>

using namespace rnqnx::views;
using namespace rnqnx::views::rnsvg;
using rnqnx::fabric::QtRnsvgImageProvider;
using rnqnx::runtime::QtImageCache;

namespace {

constexpr int kWaitMs = 5000;

QImage splitImage(int w, int h) {
  QImage image(w, h, QImage::Format_RGBA8888);
  image.fill(Qt::blue);
  QPainter p(&image);
  p.fillRect(0, 0, w / 2, h, Qt::red);
  return image;
}

QByteArray encode(const QImage& image) {
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return bytes;
}

const char kSvg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='12' height='6' viewBox='0 0 12 6'>"
    "<rect width='12' height='6' fill='#00ff00'/></svg>";

class Server final : public QTcpServer {
 public:
  Server() {
    png7x5 = encode(splitImage(7, 5));
    png3x3 = encode(splitImage(3, 3));
    if (!listen(QHostAddress::LocalHost, 0)) qFatal("local server listen failed");
    connect(this, &QTcpServer::newConnection, this, [this] {
      while (auto* socket = nextPendingConnection()) {
        auto input = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket, input] {
          input->append(socket->readAll());
          if (input->contains("\r\n\r\n")) handle(socket, QString::fromLatin1(input->split(' ').at(1)));
        });
      }
    });
  }

  QString url(const QString& path) const { return QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(path); }
  // Requests received for `path` (the query string is part of the path).
  int requests(const QString& path) const {
    const auto it = hits_.find(path);
    return it == hits_.end() ? 0 : it->second;
  }
  bool gateWaiting(const QString& path) const { return gated_.count(path) != 0; }
  // Answers a held "/gate..." request with the 7x5 PNG.
  void release(const QString& path) {
    const auto it = gated_.find(path);
    if (it == gated_.end()) return;
    QPointer<QTcpSocket> socket = it->second;
    gated_.erase(it);
    if (socket) respond(socket, "200 OK", png7x5, "image/png");
  }

  QByteArray png7x5, png3x3;

 private:
  void respond(QTcpSocket* socket, const QByteArray& status, const QByteArray& body, const QByteArray& type,
               const QByteArray& extra = {}) {
    socket->write("HTTP/1.1 " + status + "\r\n" + extra + "Content-Type: " + type +
                  "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
    socket->disconnectFromHost();
  }
  void handle(QTcpSocket* socket, const QString& path) {
    ++hits_[path];
    const QString route = path.left(path.indexOf(QLatin1Char('?')) < 0 ? path.size() : path.indexOf(QLatin1Char('?')));
    if (route.startsWith(QStringLiteral("/gate"))) {
      gated_[path] = socket;
    } else if (route == QStringLiteral("/missing.png")) {
      respond(socket, "404 Not Found", "missing", "text/plain");
    } else if (route == QStringLiteral("/broken.png")) {
      respond(socket, "200 OK", "not an image", "image/png");
    } else if (route == QStringLiteral("/redirect.png")) {
      respond(socket, "302 Found", {}, "text/plain", "Location: /ok.png\r\n");
    } else if (route == QStringLiteral("/redirect-unsafe.png")) {
      respond(socket, "302 Found", {}, "text/plain", "Location: ftp://example.invalid/image.png\r\n");
    } else if (route == QStringLiteral("/logo.svg")) {
      respond(socket, "200 OK", kSvg, "image/svg+xml");
    } else if (route == QStringLiteral("/anim.gif")) {
      respond(socket, "200 OK", QByteArray::fromBase64(
          "R0lGODlhAQABAKEAAP8AAAAA/wAAAAAAACH5BAQBAAAALAAAAAABAAEAAAICRAEAIfkEBAEAAQAsAAAAAAEAAQAAAgJMAQA7"),
          "image/gif");
    } else if (route == QStringLiteral("/small.png")) {
      respond(socket, "200 OK", png3x3, "image/png");
    } else {  // /ok.png and the dev-server style /assets/... paths
      respond(socket, "200 OK", png7x5, "image/png");
    }
  }

  std::map<QString, int> hits_;
  std::map<QString, QPointer<QTcpSocket>> gated_;
};

// One element with a binding, the way the Fabric bridge drives it.
struct Element {
  QQuickItem parent;
  RnsvgElementItem* item;
  QPointer<RnsvgElementItem> guard;
  int loadedEvents{0};
  QSize lastEventSize;

  explicit Element(QtRnsvgImageProvider& provider) : item(new RnsvgElementItem(Kind::Image, &parent)), guard(item) {
    provider_ = &provider;
  }
  void commit(const QString& uri) {
    ElementProps next;
    next.kind = Kind::Image;
    next.imageUri = uri;
    carryImageLoadState(next, item->props());
    item->setProps(std::move(next));
    auto& binding = RnsvgImageBinding::forElement(*item, *provider_);
    binding.setLoadedCallback([this](const ImageLoadedEvent& e) {
      ++loadedEvents;
      lastEventSize = e.size;
    });
    binding.setSource(uri);
  }
  ImageState state() const { return ImageState(item->props().imageState); }
  QSize imageSize() const { return item->props().image.size(); }

 private:
  QtRnsvgImageProvider* provider_{nullptr};
};

}  // namespace

class RnsvgImageHttpTest final : public QObject {
  Q_OBJECT

 private:
  std::unique_ptr<Server> server_;

 private Q_SLOTS:
  void initTestCase() { server_ = std::make_unique<Server>(); }
  void cleanupTestCase() { server_.reset(); }

  void twoElementsWithTheSameUriMakeOneRequest() {
    auto& provider = QtRnsvgImageProvider::shared();
    const QString uri = server_->url(QStringLiteral("/ok.png"));
    Element a(provider), b(provider);
    a.commit(uri);
    b.commit(uri);  // joins the fetch a started
    QCOMPARE(a.state(), ImageState::Loading);
    QCOMPARE(b.state(), ImageState::Loading);
    QCOMPARE(provider.pendingFetches(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(a.state() == ImageState::Loaded && b.state() == ImageState::Loaded, kWaitMs);
    QCOMPARE(a.imageSize(), QSize(7, 5));
    QCOMPARE(b.imageSize(), QSize(7, 5));
    QCOMPARE(server_->requests(QStringLiteral("/ok.png")), 1);
    QCOMPARE(provider.pendingFetches(), 0);
    // Both events arrive, with the decoded size.
    QTRY_COMPARE_WITH_TIMEOUT(a.loadedEvents + b.loadedEvents, 2, kWaitMs);
    QCOMPARE(a.lastEventSize, QSize(7, 5));
    // A third element, later, is served from the decoded cache: no request, and
    // loaded inside the commit.
    Element c(provider);
    c.commit(uri);
    QCOMPARE(c.state(), ImageState::Loaded);
    QCOMPARE(server_->requests(QStringLiteral("/ok.png")), 1);
  }

  void aPrefetchedImageIsReused() {
    const QString uri = server_->url(QStringLiteral("/small.png?prefetched"));
    QtImageCache prefetcher;  // what Image.prefetch uses: the shared decoded cache
    bool done = false;
    QString error;
    prefetcher.load(uri, {}, [&](QSize, QString e) { done = true; error = e; });
    QTRY_VERIFY_WITH_TIMEOUT(done, kWaitMs);
    QVERIFY(error.isEmpty());
    QCOMPARE(server_->requests(QStringLiteral("/small.png?prefetched")), 1);
    auto& provider = QtRnsvgImageProvider::shared();
    Element element(provider);
    element.commit(uri);
    QCOMPARE(element.state(), ImageState::Loaded);
    QCOMPARE(element.imageSize(), QSize(3, 3));
    QCOMPARE(server_->requests(QStringLiteral("/small.png?prefetched")), 1);
  }

  void aBundledAssetFromTheDevServerLoadsLikeAnyOtherUrl() {
    // What Image.resolveAssetSource returns when the bundle comes from Metro.
    const QString uri = server_->url(QStringLiteral("/assets/apps/rn-playground/qnx-asset.png?platform=qnx&hash=0f1e2d"));
    Element element(QtRnsvgImageProvider::shared());
    element.commit(uri);
    QTRY_COMPARE_WITH_TIMEOUT(element.state(), ImageState::Loaded, kWaitMs);
    QCOMPARE(element.imageSize(), QSize(7, 5));
  }

  void anAnimatedPrefetchStillProvidesSvgWithItsFirstFrame() {
    const QString uri = server_->url(QStringLiteral("/anim.gif"));
    QtImageCache prefetcher;
    bool done = false;
    prefetcher.load(uri, {}, [&](QSize size, QString error) {
      QVERIFY(error.isEmpty());
      QCOMPARE(size, QSize(1, 1));
      done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, kWaitMs);
    Element element(QtRnsvgImageProvider::shared());
    element.commit(uri);
    QCOMPARE(element.state(), ImageState::Loaded);
    QCOMPARE(element.imageSize(), QSize(1, 1));
    QCOMPARE(element.item->props().image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(server_->requests(QStringLiteral("/anim.gif")), 1);
  }

  void aBundledAssetBesideTheBundleLoadsFromAFileUrl() {
    // What it returns for a bundle read from disk: file://<bundle dir>/assets/<path>@2x.png
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("assets/apps/rn-playground")));
    const QString path = dir.filePath(QStringLiteral("assets/apps/rn-playground/qnx-asset@2x.png"));
    {
      QFile file(path);
      QVERIFY(file.open(QIODevice::WriteOnly));
      file.write(server_->png7x5);
    }
    Element url(QtRnsvgImageProvider::shared()), plain(QtRnsvgImageProvider::shared());
    url.commit(QUrl::fromLocalFile(path).toString());
    plain.commit(path);
    QTRY_COMPARE_WITH_TIMEOUT(url.state(), ImageState::Loaded, kWaitMs);
    QCOMPARE(url.imageSize(), QSize(7, 5));
    QTRY_COMPARE_WITH_TIMEOUT(plain.state(), ImageState::Loaded, kWaitMs);
    QCOMPARE(plain.imageSize(), QSize(7, 5));
  }

  void aSafeSameOriginRedirectLoadsItsImage() {
    Element element(QtRnsvgImageProvider::shared());
    const int targetRequests = server_->requests(QStringLiteral("/ok.png"));
    element.commit(server_->url(QStringLiteral("/redirect.png")));
    QTRY_COMPARE_WITH_TIMEOUT(element.state(), ImageState::Loaded, kWaitMs);
    QCOMPARE(element.imageSize(), QSize(7, 5));
    QCOMPARE(element.loadedEvents, 1);
    QCOMPARE(server_->requests(QStringLiteral("/redirect.png")), 1);
    QCOMPARE(server_->requests(QStringLiteral("/ok.png")), targetRequests + 1);
  }

  void httpAndFileFailuresReportAReasonAndAreNotRetried() {
    auto& provider = QtRnsvgImageProvider::shared();
    for (const char* name : {"/missing.png", "/broken.png", "/redirect-unsafe.png"}) {
      const QString path = QString::fromLatin1(name);
      Element element(provider);
      element.commit(server_->url(path));
      QTRY_COMPARE_WITH_TIMEOUT(element.state(), ImageState::Failed, kWaitMs);
      QVERIFY(element.item->props().image.isNull());
      QCOMPARE(element.loadedEvents, 0);
      QCOMPARE(server_->requests(path), 1);
      element.commit(server_->url(path));  // the next commit, same href
      QCOMPARE(server_->requests(path), 1);
    }
    Element missingFile(provider);
    missingFile.commit(QStringLiteral("/nonexistent/dir/image.png"));
    QTRY_COMPARE_WITH_TIMEOUT(missingFile.state(), ImageState::Failed, kWaitMs);
    Element badScheme(provider);
    badScheme.commit(QStringLiteral("ftp://example.invalid/a.png"));
    QTRY_COMPARE_WITH_TIMEOUT(badScheme.state(), ImageState::Failed, kWaitMs);
  }

  void aHrefChangeMidFlightShowsTheNewImageNotTheLateOne() {
    auto& provider = QtRnsvgImageProvider::shared();
    Element element(provider);
    const QString slow = server_->url(QStringLiteral("/gate-change"));
    const QString fast = server_->url(QStringLiteral("/small.png?changed"));
    element.commit(slow);
    QTRY_VERIFY_WITH_TIMEOUT(server_->gateWaiting(QStringLiteral("/gate-change")), kWaitMs);
    element.commit(fast);
    QTRY_COMPARE_WITH_TIMEOUT(element.state(), ImageState::Loaded, kWaitMs);
    QCOMPARE(element.imageSize(), QSize(3, 3));
    server_->release(QStringLiteral("/gate-change"));  // the old image arrives late
    QTRY_COMPARE_WITH_TIMEOUT(provider.pendingFetches(), 0, kWaitMs);
    QCOMPARE(element.item->props().imageUri, fast);
    QCOMPARE(element.imageSize(), QSize(3, 3));
    QCOMPARE(element.loadedEvents, 1);
  }

  void deletingAnElementMidFlightIsSafeAndOtherWaitersStillLoad() {
    auto& provider = QtRnsvgImageProvider::shared();
    const QString uri = server_->url(QStringLiteral("/gate-two"));
    auto doomed = std::make_unique<Element>(provider);
    Element survivor(provider);
    doomed->commit(uri);
    survivor.commit(uri);
    QTRY_VERIFY_WITH_TIMEOUT(server_->gateWaiting(QStringLiteral("/gate-two")), kWaitMs);
    QCOMPARE(server_->requests(QStringLiteral("/gate-two")), 1);  // one fetch for two elements
    doomed.reset();  // ASan: the response must not reach the freed element
    server_->release(QStringLiteral("/gate-two"));
    QTRY_COMPARE_WITH_TIMEOUT(survivor.state(), ImageState::Loaded, kWaitMs);
    QCOMPARE(survivor.imageSize(), QSize(7, 5));
  }

  void deletingEveryElementMidFlightLeavesNothingBehind() {
    auto& provider = QtRnsvgImageProvider::shared();
    {
      Element element(provider);
      element.commit(server_->url(QStringLiteral("/gate-alone")));
      QTRY_VERIFY_WITH_TIMEOUT(server_->gateWaiting(QStringLiteral("/gate-alone")), kWaitMs);
    }
    server_->release(QStringLiteral("/gate-alone"));
    QTRY_COMPARE_WITH_TIMEOUT(provider.pendingFetches(), 0, kWaitMs);
  }

  void aHandleMayOutliveTheProvider() {
    std::unique_ptr<rnqnx::views::rnsvg::ImageRequest> handle;
    bool called = false;
    {
      QtImageCache cache;
      QtRnsvgImageProvider local(cache);
      handle = local.load(server_->url(QStringLiteral("/gate-orphan")), [&](ImageLoadResult) { called = true; });
      QVERIFY(handle);
      QTRY_VERIFY_WITH_TIMEOUT(server_->gateWaiting(QStringLiteral("/gate-orphan")), kWaitMs);
    }
    handle.reset();  // provider and cache are gone: nothing to cancel, nothing to touch
    server_->release(QStringLiteral("/gate-orphan"));
    QCoreApplication::processEvents();
    QVERIFY(!called);
  }

  void dataUrisAreDecodedWithoutTheNetworkIncludingPercentEncodedSvg() {
    auto& provider = QtRnsvgImageProvider::shared();
    Element png(provider);
    png.commit(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(server_->png3x3.toBase64()));
    QCOMPARE(png.state(), ImageState::Loaded);
    QCOMPARE(png.imageSize(), QSize(3, 3));
    Element svg(provider);
    svg.commit(QStringLiteral("data:image/svg+xml;utf8,") + QString::fromLatin1(kSvg));
    if (svgDecodingAvailable()) {
      QCOMPARE(svg.state(), ImageState::Loaded);
      QCOMPARE(svg.imageSize(), QSize(12, 6));
    } else {
      QCOMPARE(svg.state(), ImageState::Failed);
    }
  }

  void anSvgFetchedOverHttpIsRendered() {
    if (!svgDecodingAvailable()) QSKIP("Qt's qsvg image-format plugin is not installed", SkipAll);
    Element element(QtRnsvgImageProvider::shared());
    element.commit(server_->url(QStringLiteral("/logo.svg")));
    QTRY_COMPARE_WITH_TIMEOUT(element.state(), ImageState::Loaded, kWaitMs);
    QCOMPARE(element.imageSize(), QSize(12, 6));
    QCOMPARE(element.item->props().image.pixelColor(6, 3), QColor(0, 255, 0));
  }
};

QTEST_MAIN(RnsvgImageHttpTest)
#include "RnsvgImageHttpTest.moc"
