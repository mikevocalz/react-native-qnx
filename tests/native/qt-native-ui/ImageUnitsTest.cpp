#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/image/BlurHash.h"
#include "react-native-qnx/views/image/ImageDiskCache.h"
#include "react-native-qnx/views/image/ImageEffects.h"
#include "react-native-qnx/views/image/ImageLayout.h"
#include "react-native-qnx/views/image/ImageLoadedNotifier.h"
#include "react-native-qnx/views/image/ImageRefRegistry.h"
#include "react-native-qnx/views/image/QtImageFetcher.h"
#include "react-native-qnx/views/image/ThumbHash.h"

#include <QBuffer>
#include <QGuiApplication>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace rnqnx::views;

class ImageUnitsTest : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void fitCoverContainAndFill();
  void fitNoneKeepsNaturalSizeAndScaleDownNeverGrows();
  void positionNamesOffsetsAndPercentages();
  void roundedPathClipsCornersAndLimitsRadii();
  void blurSpreadsAColorEdge();
  void tintKeepsAlphaAndReplacesColor();
  void blurHashDecodesTheReferenceVector();
  void blurHashRejectsMalformedInput();
  void thumbHashDecodesTheReferenceVectors();
  void thumbHashRejectsShortInput();
  void diskCacheEvictsLeastRecentlyUsedWithinBudget();
  void diskCacheRefusesOversizeAndClears();
  void requestKeyFoldsHeadersAndHonoursCacheKey();
  void fetcherSendsHeadersAndHitsDiskCacheSecondTime();
  void fetcherCancelNeverCallsBack();
  void decodedCacheHonorsCountAndCostLimits();
  void imageRefRegistryNeverReusesIds();
  void imageLoadedNotifierReachesListenersUntilUnsubscribed();
};

void ImageUnitsTest::fitCoverContainAndFill() {
  const QSizeF view(200, 100);
  const QSizeF image(100, 100);
  QCOMPARE(fittedRect(image, view, ContentFit::Cover, {}, 1.0), QRectF(0, -50, 200, 200));
  QCOMPARE(fittedRect(image, view, ContentFit::Contain, {}, 1.0), QRectF(50, 0, 100, 100));
  QCOMPARE(fittedRect(image, view, ContentFit::Fill, {}, 1.0), QRectF(0, 0, 200, 100));
}

void ImageUnitsTest::fitNoneKeepsNaturalSizeAndScaleDownNeverGrows() {
  const QSizeF view(200, 100);
  QCOMPARE(fittedRect(QSizeF(40, 20), view, ContentFit::None, {}, 1.0), QRectF(80, 40, 40, 20));
  QCOMPARE(fittedRect(QSizeF(40, 20), view, ContentFit::None, {}, 2.0), QRectF(90, 45, 20, 10));
  QCOMPARE(fittedRect(QSizeF(40, 20), view, ContentFit::ScaleDown, {}, 1.0), QRectF(80, 40, 40, 20));
  QCOMPARE(fittedRect(QSizeF(400, 200), view, ContentFit::ScaleDown, {}, 1.0), QRectF(0, 0, 200, 100));
}

void ImageUnitsTest::positionNamesOffsetsAndPercentages() {
  const QSizeF view(100, 100);
  const QSizeF image(50, 50);
  const auto place = [&](const QVariant& position) {
    return fittedRect(image, view, ContentFit::None, parseContentPosition(position), 1.0);
  };
  QCOMPARE(place(QStringLiteral("top left")), QRectF(0, 0, 50, 50));
  QCOMPARE(place(QStringLiteral("bottom right")), QRectF(50, 50, 50, 50));
  QCOMPARE(place(QStringLiteral("bottom")), QRectF(25, 50, 50, 50));
  QCOMPARE(place(QVariantMap{{QStringLiteral("top"), 10}, {QStringLiteral("left"), 20}}), QRectF(20, 10, 50, 50));
  QCOMPARE(place(QVariantMap{{QStringLiteral("bottom"), 10}, {QStringLiteral("right"), QStringLiteral("0%")}}),
           QRectF(50, 40, 50, 50));
  QCOMPARE(place(QVariantMap{{QStringLiteral("left"), QStringLiteral("100%")}, {QStringLiteral("top"), QStringLiteral("center")}}),
           QRectF(50, 25, 50, 50));
}

void ImageUnitsTest::roundedPathClipsCornersAndLimitsRadii() {
  const QPainterPath path = roundedRectPath(QRectF(0, 0, 100, 100), {30, 0, 0, 0});
  QVERIFY(!path.contains(QPointF(1, 1)));
  QVERIFY(path.contains(QPointF(99, 1)));
  QVERIFY(path.contains(QPointF(50, 50)));
  // Radii larger than the box scale down together and still form a closed shape.
  const QPainterPath pill = roundedRectPath(QRectF(0, 0, 100, 40), {100, 100, 100, 100});
  QVERIFY(pill.contains(QPointF(50, 20)));
  QVERIFY(!pill.contains(QPointF(1, 1)));
}

void ImageUnitsTest::blurSpreadsAColorEdge() {
  QImage image(40, 10, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::black);
  for (int y = 0; y < 10; ++y)
    for (int x = 20; x < 40; ++x) image.setPixel(x, y, qRgb(255, 255, 255));
  const QImage blurred = blurImage(image, 4);
  const int near = qRed(blurred.pixel(20, 5));
  QVERIFY2(near > 40 && near < 215, qPrintable(QString::number(near)));
  QCOMPARE(qRed(blurred.pixel(0, 5)), 0);
  QCOMPARE(qRed(blurred.pixel(39, 5)), 255);
  QCOMPARE(blurImage(image, 0), image);
  // A huge radius from JavaScript is capped: it returns a bounded result at once.
  QElapsedTimer timer;
  timer.start();
  const QImage huge = blurImage(image, 1000000);
  QVERIFY(timer.elapsed() < 2000);
  QCOMPARE(huge.size(), image.size());
}

void ImageUnitsTest::tintKeepsAlphaAndReplacesColor() {
  QImage image(2, 1, QImage::Format_ARGB32_Premultiplied);
  image.setPixel(0, 0, qRgba(10, 200, 30, 255));
  image.setPixel(1, 0, qRgba(0, 0, 0, 0));
  const QImage tinted = tintImage(image, QColor(255, 0, 0));
  QCOMPARE(tinted.pixelColor(0, 0), QColor(255, 0, 0, 255));
  QCOMPARE(tinted.pixelColor(1, 0).alpha(), 0);
}

void ImageUnitsTest::blurHashDecodesTheReferenceVector() {
  // One component: the whole image is the DC color. "00TI:j" encodes sRGB (255, 0, 0).
  const QImage red = decodeBlurHash(QStringLiteral("00TI:j"), 4, 4);
  QVERIFY(!red.isNull());
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) QCOMPARE(red.pixelColor(x, y), QColor(255, 0, 0));
  // The 4x3 example from the BlurHash README decodes at the requested size.
  const QImage image = decodeBlurHash(QStringLiteral("LEHV6nWB2yk8pyo0adR*.7kCMdnj"), 32, 24);
  QCOMPARE(image.size(), QSize(32, 24));
  // Its DC term is sRGB (151, 150, 149); AC terms average out over the image.
  quint64 sum = 0;
  for (int y = 0; y < 24; ++y)
    for (int x = 0; x < 32; ++x) sum += static_cast<quint64>(image.pixelColor(x, y).red());
  const quint64 mean = sum / (32 * 24);
  QVERIFY2(mean > 90 && mean < 210, qPrintable(QString::number(mean)));
}

void ImageUnitsTest::blurHashRejectsMalformedInput() {
  QVERIFY(decodeBlurHash(QStringLiteral(""), 8, 8).isNull());
  QVERIFY(decodeBlurHash(QStringLiteral("LEHV6nWB2yk8pyo0adR*.7kCMdn"), 8, 8).isNull());  // truncated
  QVERIFY(decodeBlurHash(QStringLiteral("LEHV6nWB2yk8pyo0adR*.7kCMdnj"), 0, 8).isNull());
  QVERIFY(decodeBlurHash(QStringLiteral("LEHV6nWB2yk8pyo0 dR*.7kCMdnj"), 8, 8).isNull());  // bad digit
}

void ImageUnitsTest::thumbHashDecodesTheReferenceVectors() {
  // Opaque hash from the ThumbHash README.
  const QImage opaque = decodeThumbHash(QStringLiteral("1QcSHQRnh493V4dIh4eXh1h4kJUI"));
  QVERIFY(!opaque.isNull());
  QVERIFY(opaque.width() <= 32 && opaque.height() <= 32);
  QVERIFY(opaque.width() == 32 || opaque.height() == 32);
  QCOMPARE(opaque.pixelColor(opaque.width() / 2, opaque.height() / 2).alpha(), 255);
  // A hash with the alpha bit (header byte 2 high bit) decodes with alpha.
  QByteArray alpha(40, '\x42');
  alpha[2] = static_cast<char>(alpha[2] | 0x80);
  const QImage withAlpha = decodeThumbHash(alpha);
  QVERIFY(!withAlpha.isNull());
}

void ImageUnitsTest::thumbHashRejectsShortInput() {
  QVERIFY(decodeThumbHash(QByteArray("abc")).isNull());
  QVERIFY(decodeThumbHash(QStringLiteral("")).isNull());
  // Header only: the AC coefficients are missing.
  QVERIFY(decodeThumbHash(QByteArray(5, '\x11')).isNull());
}

void ImageUnitsTest::diskCacheEvictsLeastRecentlyUsedWithinBudget() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  ImageDiskCache cache(dir.path(), 300);
  const QByteArray block(100, 'x');
  QVERIFY(cache.write(QStringLiteral("a"), block));
  QTest::qWait(20);
  QVERIFY(cache.write(QStringLiteral("b"), block));
  QTest::qWait(20);
  QVERIFY(cache.write(QStringLiteral("c"), block));
  QTest::qWait(20);
  QCOMPARE(cache.read(QStringLiteral("a")), block);  // a becomes the newest
  QTest::qWait(20);
  QVERIFY(cache.write(QStringLiteral("d"), block));  // over budget: b goes
  QVERIFY(!cache.contains(QStringLiteral("b")));
  QVERIFY(cache.contains(QStringLiteral("a")));
  QVERIFY(cache.contains(QStringLiteral("c")));
  QVERIFY(cache.contains(QStringLiteral("d")));
  QVERIFY(cache.usedBytes() <= 300);
  cache.setBudgetBytes(100);
  QVERIFY(cache.usedBytes() <= 100);
}

void ImageUnitsTest::diskCacheRefusesOversizeAndClears() {
  QTemporaryDir dir;
  ImageDiskCache cache(dir.path(), 50);
  QVERIFY(!cache.write(QStringLiteral("big"), QByteArray(51, 'x')));
  QVERIFY(!cache.contains(QStringLiteral("big")));
  QVERIFY(cache.write(QStringLiteral("ok"), QByteArray(10, 'x')));
  QVERIFY(QFileInfo::exists(cache.pathFor(QStringLiteral("ok"))));
  cache.clear();
  QCOMPARE(cache.usedBytes(), 0);
  QVERIFY(cache.read(QStringLiteral("ok")).isEmpty());
}

void ImageUnitsTest::requestKeyFoldsHeadersAndHonoursCacheKey() {
  ImageRequest plain;
  plain.uri = QStringLiteral("https://a.example/x.png");
  QCOMPARE(plain.storageKey(), plain.uri);
  ImageRequest one = plain;
  one.headers = {{"Authorization", "one"}};
  ImageRequest two = plain;
  two.headers = {{"Authorization", "two"}};
  QVERIFY(one.storageKey() != plain.storageKey());
  QVERIFY(one.storageKey() != two.storageKey());
  ImageRequest keyed = one;
  keyed.cacheKey = QStringLiteral("stable");
  ImageRequest keyedOther = two;
  keyedOther.cacheKey = QStringLiteral("stable");
  QCOMPARE(keyed.storageKey(), keyedOther.storageKey());
}

namespace {

QByteArray pngBytes() {
  QImage image(7, 5, QImage::Format_RGBA8888);
  image.fill(Qt::red);
  QByteArray png;
  QBuffer out(&png);
  out.open(QIODevice::WriteOnly);
  image.save(&out, "PNG");
  return png;
}

class HeaderServer : public QTcpServer {
 public:
  QByteArray png{pngBytes()};
  int requests{0};
  QByteArray lastRequest;
  HeaderServer() {
    listen(QHostAddress::LocalHost, 0);
    QObject::connect(this, &QTcpServer::newConnection, this, [this] {
      while (auto* socket = nextPendingConnection()) {
        auto input = std::make_shared<QByteArray>();
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, input] {
          input->append(socket->readAll());
          if (!input->contains("\r\n\r\n")) return;
          ++requests;
          lastRequest = *input;
          socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " +
                        QByteArray::number(png.size()) + "\r\nConnection: close\r\n\r\n" + png);
          socket->disconnectFromHost();
        });
      }
    });
  }
  QString url() const { return QStringLiteral("http://127.0.0.1:%1/pic.png").arg(serverPort()); }
};

FetchResult await(QtImageFetcher& fetcher, const ImageRequest& request) {
  FetchResult out;
  bool done = false;
  fetcher.fetch(request, {}, [&](FetchResult result) {
    out = std::move(result);
    done = true;
  });
  QElapsedTimer clock;
  clock.start();
  while (!done && clock.elapsed() < 5000) QTest::qWait(5);
  if (!done) out.error = QStringLiteral("timed out");
  return out;
}

}  // namespace

void ImageUnitsTest::fetcherSendsHeadersAndHitsDiskCacheSecondTime() {
  HeaderServer server;
  QTemporaryDir dir;
  ImageDiskCache disk(dir.path(), 1 << 20);
  QtImageFetcher fetcher(5000, &disk);
  ImageRequest request;
  request.uri = server.url();
  request.headers = {{"X-Token", "abc123"}};
  request.policy = ImageCachePolicy::MemoryDisk;

  const FetchResult first = await(fetcher, request);
  QVERIFY2(first.error.isEmpty(), qPrintable(first.error));
  QCOMPARE(first.origin, ImageOrigin::Network);
  QCOMPARE(first.bytes, server.png);
  QVERIFY2(server.lastRequest.toLower().contains("x-token: abc123"), server.lastRequest.constData());
  QCOMPARE(server.requests, 1);

  const FetchResult second = await(fetcher, request);
  QCOMPARE(second.origin, ImageOrigin::Disk);
  QCOMPARE(second.bytes, server.png);
  QCOMPARE(server.requests, 1);

  // Different headers are a different cache entry.
  request.headers = {{"X-Token", "other"}};
  const FetchResult third = await(fetcher, request);
  QCOMPARE(third.origin, ImageOrigin::Network);
  QCOMPARE(server.requests, 2);

  // cachePolicy none never reads or writes the disk.
  ImageRequest bypass;
  bypass.uri = server.url();
  bypass.policy = ImageCachePolicy::None;
  QCOMPARE(await(fetcher, bypass).origin, ImageOrigin::Network);
  QCOMPARE(await(fetcher, bypass).origin, ImageOrigin::Network);
  QVERIFY(!disk.contains(bypass.storageKey()));
}

void ImageUnitsTest::fetcherCancelNeverCallsBack() {
  QTcpServer silent;
  QVERIFY(silent.listen(QHostAddress::LocalHost, 0));
  QtImageFetcher fetcher(5000, nullptr);
  bool called = false;
  ImageRequest request;
  request.uri = QStringLiteral("http://127.0.0.1:%1/never.png").arg(silent.serverPort());
  const auto id = fetcher.fetch(request, {}, [&](FetchResult) { called = true; });
  QTest::qWait(100);
  fetcher.cancel(id);
  QTest::qWait(200);
  QVERIFY(!called);
}

void ImageUnitsTest::decodedCacheHonorsCountAndCostLimits() {
  DecodedImageCache cache(1024);
  QImage image(8, 8, QImage::Format_ARGB32);
  image.fill(Qt::red);
  cache.configure(0, 2);
  cache.insert(QStringLiteral("a"), image);
  cache.insert(QStringLiteral("b"), image);
  cache.insert(QStringLiteral("c"), image);
  QCOMPARE(cache.count(), 2);
  QVERIFY(!cache.contains(QStringLiteral("a")));  // the oldest went first
  QVERIFY(cache.contains(QStringLiteral("b")));
  QVERIFY(cache.contains(QStringLiteral("c")));
  cache.configure(0, 1);  // lowering the count trims at once
  QCOMPARE(cache.count(), 1);
  QVERIFY(cache.contains(QStringLiteral("c")));
  cache.configure(0, 0);  // 0 is no limit
  cache.insert(QStringLiteral("d"), image);
  cache.insert(QStringLiteral("e"), image);
  QCOMPARE(cache.count(), 3);
  cache.configure(7, 0);
  QCOMPARE(cache.maxCostKiB(), 7);
  cache.configure(0, 0);  // 0 keeps the cost ceiling
  QCOMPARE(cache.maxCostKiB(), 7);
  QImage large(64, 64, QImage::Format_ARGB32);  // 16 KiB: over the 7 KiB ceiling
  cache.insert(QStringLiteral("big"), large);
  QVERIFY(!cache.contains(QStringLiteral("big")));
}

void ImageUnitsTest::imageRefRegistryNeverReusesIds() {
  auto& registry = ImageRefRegistry::shared();
  ImageRefEntry entry;
  entry.image = QImage(2, 2, QImage::Format_ARGB32);
  entry.mediaType = QStringLiteral("image/png");
  const auto first = registry.add(entry);
  QVERIFY(first != 0);
  QCOMPARE(registry.find(first).image.size(), QSize(2, 2));
  QCOMPARE(registry.find(first).mediaType, QStringLiteral("image/png"));
  QVERIFY(registry.release(first));
  QVERIFY(!registry.release(first));
  QVERIFY(registry.find(first).image.isNull());
  QVERIFY(registry.add(entry) != first);
  QVERIFY(registry.find(0).image.isNull());
}

void ImageUnitsTest::imageLoadedNotifierReachesListenersUntilUnsubscribed() {
  auto& notifier = ImageLoadedNotifier::shared();
  QStringList urls;
  const auto token = notifier.subscribe([&](const ImageLoadedEvent& event) {
    urls.push_back(QStringLiteral("%1 %2x%3").arg(event.url).arg(event.width).arg(event.height));
  });
  notifier.notify({QStringLiteral("a.png"), 4, 3});
  notifier.unsubscribe(token);
  notifier.notify({QStringLiteral("b.png"), 1, 1});
  QCOMPARE(urls, QStringList{QStringLiteral("a.png 4x3")});
}

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  ImageUnitsTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "ImageUnitsTest.moc"
