// Host tests for the ExpoImage view: it loads, decodes and paints real files
// and an HTTP server in an offscreen QQuickWindow, and the assertions read
// pixels from the painted result.
#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/image/ExpoImageItem.h"
#include "react-native-qnx/views/image/ImageDiskCache.h"
#include "react-native-qnx/views/image/ImageRefRegistry.h"

#include <QBuffer>
#include <QGuiApplication>
#include <QHostAddress>
#include <QImage>
#include <QMovie>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QSemaphore>
#include <QThreadPool>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

using namespace rnqnx::views;

namespace {

QString writePng(const QTemporaryDir& dir, const QString& name, const QSize& size, const QColor& color) {
  QImage image(size, QImage::Format_ARGB32);
  image.fill(color);
  const QString path = dir.filePath(name);
  image.save(path);
  return path;
}

QVariantList sourceOf(const QString& path) {
  return {QVariantMap{{QStringLiteral("uri"), QUrl::fromLocalFile(path).toString()}}};
}

QColor at(const QImage& image, int x, int y) { return image.pixelColor(x, y); }

bool near(const QColor& a, const QColor& b, int tolerance = 6) {
  return std::abs(a.red() - b.red()) <= tolerance && std::abs(a.green() - b.green()) <= tolerance &&
         std::abs(a.blue() - b.blue()) <= tolerance && std::abs(a.alpha() - b.alpha()) <= tolerance;
}

}  // namespace

class ExpoImageItemTest : public QObject {
  Q_OBJECT

  QQuickWindow window_;
  QTemporaryDir dir_;
  std::unique_ptr<ExpoImageItem> item_;

  void make(const QSizeF& size = QSizeF(40, 40)) {
    item_ = std::make_unique<ExpoImageItem>(window_.contentItem());
    item_->setSize(size);
  }
  void loadAndWait(const QVariantList& source) {
    QSignalSpy displayed(item_.get(), &ExpoImageItem::displayedChanged);
    item_->setSource(source);
    QTRY_VERIFY_WITH_TIMEOUT(item_->displayed(), 5000);
  }

 private Q_SLOTS:
  void initTestCase() {
    ExpoImageItem::registerQmlType();
    window_.resize(200, 200);
    window_.show();
    QVERIFY(dir_.isValid());
  }
  void init() {
    DecodedImageCache::shared().clear();
    ImageDiskCache::shared().clear();
    item_.reset();
  }

  void borderRadiusRoundsThePicture();
  void perCornerRadiiRoundOnlyThoseCorners();
  void tintReplacesColorAndKeepsShape();
  void blurSoftensAHardEdge();
  void contentFitNoneDrawsNaturalSizeCentered();
  void contentPositionMovesTheImage();
  void blurhashPlaceholderShowsUntilTheImageArrives();
  void thumbhashSourceDrawsWithoutNetwork();
  void crossDissolveBlendsOldAndNew();
  void flipEffectsPassThroughAnEdgeOnMidpoint();
  void curlFallsBackToCrossDissolve();
  void multipleSourcesPickTheSmallestThatCovers();
  void onLoadReportsSizeCacheTypeAndMediaType();
  void secondViewReadsFromTheSharedMemoryCache();
  void headersReachTheServerAndKeyTheCache();
  void failedLoadReportsTheError();
  void recyclingKeyClearsThePreviousPicture();
  void animatedGifPlaysAndAutoplayFalsePauses();
  void hitSlopExtendsAndPointerEventsNoneIgnoresTouches();
  void downscalingKeepsReportedSize();
  void growingADownscaledViewDecodesEnoughPixels();
  void lockedGrowthDefersDecodeUntilUnlock();
  void upgradingDecodeOptions_data();
  void upgradingDecodeOptions();
  void growthDuringInitialDecodeIsRechecked();
  void freshViewReadsTheDiskCacheWhenMemoryIsCleared();
  void imageRefSourceShowsTheDecodedPicture();
  void lockedViewKeepsItsPictureUntilUnlockedOrReloaded();
};

void ExpoImageItemTest::borderRadiusRoundsThePicture() {
  make();
  item_->setRadius(16);
  loadAndWait(sourceOf(writePng(dir_, "red.png", QSize(40, 40), Qt::red)));
  const QImage out = item_->grabImage();
  QCOMPARE(at(out, 0, 0).alpha(), 0);      // corner is clipped
  QCOMPARE(at(out, 39, 39).alpha(), 0);
  QVERIFY(near(at(out, 20, 20), Qt::red));  // center is the picture
  QVERIFY(near(at(out, 20, 1), Qt::red));   // straight edges are not clipped
}

void ExpoImageItemTest::perCornerRadiiRoundOnlyThoseCorners() {
  make();
  item_->setUsePerCornerRadius(true);
  item_->setTopLeftRadius(20);
  loadAndWait(sourceOf(writePng(dir_, "blue.png", QSize(40, 40), Qt::blue)));
  const QImage out = item_->grabImage();
  QCOMPARE(at(out, 0, 0).alpha(), 0);
  QVERIFY(near(at(out, 39, 0), Qt::blue));
  QVERIFY(near(at(out, 0, 39), Qt::blue));
  QVERIFY(near(at(out, 39, 39), Qt::blue));
}

void ExpoImageItemTest::tintReplacesColorAndKeepsShape() {
  make();
  item_->setHasTint(true);
  item_->setTintColor(QColor(0, 255, 0));
  item_->setRadius(16);
  loadAndWait(sourceOf(writePng(dir_, "red2.png", QSize(40, 40), Qt::red)));
  const QImage out = item_->grabImage();
  QVERIFY(near(at(out, 20, 20), QColor(0, 255, 0)));
  QCOMPARE(at(out, 0, 0).alpha(), 0);
}

void ExpoImageItemTest::blurSoftensAHardEdge() {
  QImage half(40, 40, QImage::Format_ARGB32);
  half.fill(Qt::black);
  for (int y = 0; y < 40; ++y)
    for (int x = 20; x < 40; ++x) half.setPixelColor(x, y, Qt::white);
  const QString path = dir_.filePath("half.png");
  half.save(path);
  make();
  item_->setContentFit(QStringLiteral("fill"));
  loadAndWait(sourceOf(path));
  const int sharp = at(item_->grabImage(), 19, 20).red();
  item_->setBlurRadius(6);
  const QImage blurred = item_->grabImage();
  QVERIFY(sharp < 10);
  QVERIFY2(at(blurred, 19, 20).red() > 40, qPrintable(QString::number(at(blurred, 19, 20).red())));
  QVERIFY(at(blurred, 2, 20).red() < 10);
}

void ExpoImageItemTest::contentFitNoneDrawsNaturalSizeCentered() {
  make(QSizeF(40, 40));
  item_->setContentFit(QStringLiteral("none"));
  item_->setBackgroundColor(Qt::white);
  loadAndWait(sourceOf(writePng(dir_, "small.png", QSize(10, 10), Qt::red)));
  const QImage out = item_->grabImage();
  QVERIFY(near(at(out, 20, 20), Qt::red));
  QVERIFY(near(at(out, 5, 5), Qt::white));
  QVERIFY(near(at(out, 34, 34), Qt::white));
}

void ExpoImageItemTest::contentPositionMovesTheImage() {
  make(QSizeF(40, 40));
  item_->setContentFit(QStringLiteral("none"));
  item_->setContentPosition(QStringLiteral("top left"));
  item_->setBackgroundColor(Qt::white);
  loadAndWait(sourceOf(writePng(dir_, "small2.png", QSize(10, 10), Qt::red)));
  const QImage out = item_->grabImage();
  QVERIFY(near(at(out, 4, 4), Qt::red));
  QVERIFY(near(at(out, 20, 20), Qt::white));
}

void ExpoImageItemTest::blurhashPlaceholderShowsUntilTheImageArrives() {
  make();
  item_->setPlaceholder({QVariantMap{{QStringLiteral("blurhash"), QStringLiteral("00TI:j")}}});
  // A source that never resolves keeps the placeholder on screen.
  QTcpServer silent;
  QVERIFY(silent.listen(QHostAddress::LocalHost, 0));
  item_->setSource({QVariantMap{{QStringLiteral("uri"),
                                 QStringLiteral("http://127.0.0.1:%1/slow.png").arg(silent.serverPort())}}});
  QTRY_VERIFY_WITH_TIMEOUT(near(at(item_->grabImage(), 20, 20), Qt::red, 12), 3000);
  QVERIFY(!item_->displayed());
}

void ExpoImageItemTest::thumbhashSourceDrawsWithoutNetwork() {
  make();
  item_->setContentFit(QStringLiteral("fill"));
  loadAndWait({QVariantMap{{QStringLiteral("thumbhash"), QStringLiteral("1QcSHQRnh493V4dIh4eXh1h4kJUI")}}});
  const QImage out = item_->grabImage();
  QVERIFY(at(out, 20, 20).alpha() == 255);
}

void ExpoImageItemTest::crossDissolveBlendsOldAndNew() {
  make();
  QVariantMap transition{{QStringLiteral("duration"), 400}, {QStringLiteral("effect"), QStringLiteral("cross-dissolve")},
                         {QStringLiteral("timing"), QStringLiteral("linear")}};
  item_->setTransition(transition);
  loadAndWait(sourceOf(writePng(dir_, "r.png", QSize(40, 40), Qt::red)));
  item_->setSource(sourceOf(writePng(dir_, "g.png", QSize(40, 40), QColor(0, 0, 255))));
  QSignalSpy displayed(item_.get(), &ExpoImageItem::displayedChanged);
  // Mid-transition the pixel is neither pure red nor pure blue.
  bool mixed = false;
  QElapsedTimer clock;
  clock.start();
  while (clock.elapsed() < 1500 && !mixed) {
    QTest::qWait(20);
    const QColor c = at(item_->grabImage(), 20, 20);
    mixed = c.red() > 40 && c.blue() > 40;
  }
  QVERIFY(mixed);
  QTRY_VERIFY_WITH_TIMEOUT(item_->displayed(), 3000);
  QVERIFY(near(at(item_->grabImage(), 20, 20), QColor(0, 0, 255)));
}

void ExpoImageItemTest::flipEffectsPassThroughAnEdgeOnMidpoint() {
  for (const char* effect : {"flip-from-top", "flip-from-bottom", "flip-from-left", "flip-from-right"}) {
    make();
    item_->setBackgroundColor(Qt::white);
    item_->setTransition({{QStringLiteral("duration"), 500}, {QStringLiteral("effect"), QString::fromLatin1(effect)},
                          {QStringLiteral("timing"), QStringLiteral("linear")}});
    loadAndWait(sourceOf(writePng(dir_, "fr.png", QSize(40, 40), Qt::red)));
    item_->setSource(sourceOf(writePng(dir_, "fb.png", QSize(40, 40), Qt::blue)));
    // At some moment the picture is nearly edge-on: the corners show the background.
    bool edgeOn = false;
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < 1500 && !edgeOn) {
      QTest::qWait(15);
      const QImage out = item_->grabImage();
      edgeOn = near(at(out, 2, 2), Qt::white, 8) && near(at(out, 37, 37), Qt::white, 8);
    }
    QVERIFY2(edgeOn, effect);
    QTRY_VERIFY_WITH_TIMEOUT(item_->displayed(), 3000);
    QVERIFY2(near(at(item_->grabImage(), 20, 20), Qt::blue), effect);
    init();
  }
}

void ExpoImageItemTest::curlFallsBackToCrossDissolve() {
  make();
  QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("curl-up.*iOS-only")));
  item_->setTransition({{QStringLiteral("duration"), 100}, {QStringLiteral("effect"), QStringLiteral("curl-up")}});
  loadAndWait(sourceOf(writePng(dir_, "cu.png", QSize(40, 40), Qt::red)));
  QVERIFY(near(at(item_->grabImage(), 20, 20), Qt::red));
}

void ExpoImageItemTest::multipleSourcesPickTheSmallestThatCovers() {
  make(QSizeF(40, 40));
  const QString small = writePng(dir_, "s1.png", QSize(20, 20), Qt::red);
  const QString medium = writePng(dir_, "s2.png", QSize(50, 50), Qt::green);
  const QString large = writePng(dir_, "s3.png", QSize(200, 200), Qt::blue);
  const auto entry = [](const QString& path, int width) {
    return QVariantMap{{QStringLiteral("uri"), QUrl::fromLocalFile(path).toString()}, {QStringLiteral("width"), width}};
  };
  loadAndWait({entry(small, 20), entry(large, 200), entry(medium, 50)});
  QVERIFY(item_->shownUri().endsWith(QStringLiteral("s2.png")));
  QVERIFY(near(at(item_->grabImage(), 20, 20), Qt::green));
}

void ExpoImageItemTest::onLoadReportsSizeCacheTypeAndMediaType() {
  make();
  QSignalSpy loaded(item_.get(), &ExpoImageItem::loaded);
  QSignalSpy started(item_.get(), &ExpoImageItem::loadStarted);
  loadAndWait(sourceOf(writePng(dir_, "meta.png", QSize(30, 20), Qt::red)));
  QCOMPARE(started.count(), 1);
  QCOMPARE(loaded.count(), 1);
  const auto args = loaded.takeFirst();
  QCOMPARE(args.at(1).toDouble(), 30.0);
  QCOMPARE(args.at(2).toDouble(), 20.0);
  QCOMPARE(args.at(3).toString(), QStringLiteral("none"));
  QCOMPARE(args.at(4).toString(), QStringLiteral("image/png"));
  QCOMPARE(args.at(5).toBool(), false);
}

void ExpoImageItemTest::secondViewReadsFromTheSharedMemoryCache() {
  const QString path = writePng(dir_, "shared.png", QSize(16, 16), Qt::red);
  make();
  loadAndWait(sourceOf(path));
  auto second = std::make_unique<ExpoImageItem>(window_.contentItem());
  second->setSize(QSizeF(40, 40));
  QSignalSpy loaded(second.get(), &ExpoImageItem::loaded);
  second->setSource(sourceOf(path));
  QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 3000);
  QCOMPARE(loaded.first().at(3).toString(), QStringLiteral("memory"));
}

namespace {
class PictureServer : public QTcpServer {
 public:
  QByteArray png;
  QList<QByteArray> requests;
  explicit PictureServer(QColor color) {
    QImage image(8, 8, QImage::Format_RGBA8888);
    image.fill(color);
    QBuffer out(&png);
    out.open(QIODevice::WriteOnly);
    image.save(&out, "PNG");
    listen(QHostAddress::LocalHost, 0);
    QObject::connect(this, &QTcpServer::newConnection, this, [this] {
      while (auto* socket = nextPendingConnection()) {
        auto input = std::make_shared<QByteArray>();
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, input] {
          input->append(socket->readAll());
          if (!input->contains("\r\n\r\n")) return;
          requests.append(*input);
          const bool authorized = input->toLower().contains("authorization: bearer good");
          if (!authorized) {
            socket->write("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
          } else {
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " +
                          QByteArray::number(png.size()) + "\r\nConnection: close\r\n\r\n" + png);
          }
          socket->disconnectFromHost();
        });
      }
    });
  }
  QString url() const { return QStringLiteral("http://127.0.0.1:%1/p.png").arg(serverPort()); }
};
}  // namespace

void ExpoImageItemTest::headersReachTheServerAndKeyTheCache() {
  PictureServer server(Qt::red);
  make();
  QSignalSpy failed(item_.get(), &ExpoImageItem::failed);
  const auto withToken = [&](const QString& token) {
    return QVariantList{QVariantMap{{QStringLiteral("uri"), server.url()},
                                    {QStringLiteral("headers"), QVariantMap{{QStringLiteral("Authorization"), token}}}}};
  };
  item_->setSource(withToken(QStringLiteral("Bearer bad")));
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
  QVERIFY2(failed.first().first().toString().contains(QStringLiteral("401")), "401 surfaces as the error");
  loadAndWait(withToken(QStringLiteral("Bearer good")));
  QVERIFY(near(at(item_->grabImage(), 20, 20), Qt::red));
  // The good-token picture is cached under the headers: a second view with
  // the same URL and headers is served from memory, a view without them is not.
  auto same = std::make_unique<ExpoImageItem>(window_.contentItem());
  same->setSize(QSizeF(40, 40));
  QSignalSpy sameLoaded(same.get(), &ExpoImageItem::loaded);
  same->setSource(withToken(QStringLiteral("Bearer good")));
  QTRY_COMPARE_WITH_TIMEOUT(sameLoaded.count(), 1, 3000);
  QCOMPARE(sameLoaded.first().at(3).toString(), QStringLiteral("memory"));
  auto bare = std::make_unique<ExpoImageItem>(window_.contentItem());
  bare->setSize(QSizeF(40, 40));
  QSignalSpy bareFailed(bare.get(), &ExpoImageItem::failed);
  bare->setSource({QVariantMap{{QStringLiteral("uri"), server.url()}}});
  QTRY_COMPARE_WITH_TIMEOUT(bareFailed.count(), 1, 5000);
}

void ExpoImageItemTest::freshViewReadsTheDiskCacheWhenMemoryIsCleared() {
  PictureServer server(Qt::red);
  const QVariantList source{QVariantMap{
      {QStringLiteral("uri"), server.url()},
      {QStringLiteral("headers"), QVariantMap{{QStringLiteral("Authorization"), QStringLiteral("Bearer good")}}}}};
  make();
  QSignalSpy first(item_.get(), &ExpoImageItem::loaded);
  loadAndWait(source);
  QCOMPARE(first.first().at(3).toString(), QStringLiteral("none"));
  QCOMPARE(server.requests.size(), 1);
  DecodedImageCache::shared().clear();
  auto fresh = std::make_unique<ExpoImageItem>(window_.contentItem());
  fresh->setSize(QSizeF(40, 40));
  QSignalSpy loaded(fresh.get(), &ExpoImageItem::loaded);
  fresh->setSource(source);
  QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 3000);
  QCOMPARE(loaded.first().at(3).toString(), QStringLiteral("disk"));
  QCOMPARE(server.requests.size(), 1);  // no second request
  // cachePolicy none bypasses both.
  DecodedImageCache::shared().clear();
  auto uncached = std::make_unique<ExpoImageItem>(window_.contentItem());
  uncached->setSize(QSizeF(40, 40));
  uncached->setCachePolicy(QStringLiteral("none"));
  QSignalSpy uncachedLoaded(uncached.get(), &ExpoImageItem::loaded);
  uncached->setSource(source);
  QTRY_COMPARE_WITH_TIMEOUT(uncachedLoaded.count(), 1, 3000);
  QCOMPARE(uncachedLoaded.first().at(3).toString(), QStringLiteral("none"));
  QCOMPARE(server.requests.size(), 2);
}

void ExpoImageItemTest::imageRefSourceShowsTheDecodedPicture() {
  QImage picture(8, 8, QImage::Format_ARGB32);
  picture.fill(QColor(0, 200, 0));
  ImageRefEntry entry;
  entry.image = picture;
  entry.mediaType = QStringLiteral("image/png");
  const auto id = ImageRefRegistry::shared().add(entry);
  make();
  QSignalSpy loaded(item_.get(), &ExpoImageItem::loaded);
  loadAndWait({QVariantMap{{QStringLiteral("imageRef"), id}}});
  QCOMPARE(loaded.first().at(2).toDouble(), 8.0);
  QCOMPARE(loaded.first().at(4).toString(), QStringLiteral("image/png"));
  QVERIFY(near(at(item_->grabImage(), 20, 20), QColor(0, 200, 0)));
  // A released ref fails with a message instead of showing a stale picture.
  ImageRefRegistry::shared().release(id);
  QSignalSpy failed(item_.get(), &ExpoImageItem::failed);
  item_->reload();
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 3000);
  // As a placeholder it paints until the real source arrives.
  const auto placeholderId = ImageRefRegistry::shared().add(entry);
  auto withPlaceholder = std::make_unique<ExpoImageItem>(window_.contentItem());
  withPlaceholder->setSize(QSizeF(40, 40));
  withPlaceholder->setPlaceholder({QVariantMap{{QStringLiteral("imageRef"), placeholderId}}});
  withPlaceholder->setSource({QVariantMap{{QStringLiteral("uri"), QStringLiteral("http://127.0.0.1:1/never.png")}}});
  QTRY_VERIFY_WITH_TIMEOUT(near(at(withPlaceholder->grabImage(), 20, 20), QColor(0, 200, 0)), 3000);
}

void ExpoImageItemTest::lockedViewKeepsItsPictureUntilUnlockedOrReloaded() {
  const QString red = writePng(dir_, "lock-red.png", QSize(20, 20), Qt::red);
  const QString blue = writePng(dir_, "lock-blue.png", QSize(20, 20), Qt::blue);
  make();
  loadAndWait(sourceOf(red));
  item_->lockResource();
  QVERIFY(item_->resourceLocked());
  item_->setSource(sourceOf(blue));
  QTest::qWait(300);
  QVERIFY(near(at(item_->grabImage(), 20, 20), Qt::red));  // still the first picture
  QVERIFY(item_->shownUri().endsWith(QStringLiteral("lock-red.png")));
  item_->unlockResource();
  QTRY_VERIFY_WITH_TIMEOUT(near(at(item_->grabImage(), 20, 20), Qt::blue), 3000);
  // reload ignores the lock.
  item_->lockResource();
  QSignalSpy loaded(item_.get(), &ExpoImageItem::loaded);
  item_->reload();
  QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 3000);
  item_->unlockResource();
}

void ExpoImageItemTest::failedLoadReportsTheError() {
  make();
  QSignalSpy failed(item_.get(), &ExpoImageItem::failed);
  item_->setSource(sourceOf(dir_.filePath("missing.png")));
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 3000);
  QVERIFY(!failed.first().first().toString().isEmpty());
  QVERIFY(!item_->displayed());
}

void ExpoImageItemTest::recyclingKeyClearsThePreviousPicture() {
  make();
  item_->setRecyclingKey(QStringLiteral("a"));
  loadAndWait(sourceOf(writePng(dir_, "rk.png", QSize(40, 40), Qt::red)));
  QVERIFY(near(at(item_->grabImage(), 20, 20), Qt::red));
  item_->setRecyclingKey(QStringLiteral("b"));
  QCOMPARE(at(item_->grabImage(), 20, 20).alpha(), 0);
  QVERIFY(!item_->displayed());
}

void ExpoImageItemTest::animatedGifPlaysAndAutoplayFalsePauses() {
  const QString path = dir_.filePath("anim.gif");
  {
    // Two-frame GIF written with QMovie-compatible encoder is unavailable in Qt;
    // use a prebuilt 2-frame GIF (red, blue), 1x1 px, 10 ms per frame.
    static const unsigned char gif[] = {
        0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0xA1, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0xF9, 0x04, 0x04, 0x01, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x21, 0xF9, 0x04, 0x04, 0x01, 0x00, 0x01, 0x00, 0x2C,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x4C, 0x01, 0x00, 0x3B};
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(reinterpret_cast<const char*>(gif), sizeof(gif));
  }
  QMovie probe(path);
  if (!probe.isValid() || probe.frameCount() < 2) QSKIP("the Qt build has no animated GIF support");
  make();
  item_->setContentFit(QStringLiteral("fill"));
  QSignalSpy loaded(item_.get(), &ExpoImageItem::loaded);
  loadAndWait(sourceOf(path));
  QCOMPARE(loaded.first().at(5).toBool(), true);
  QTRY_VERIFY_WITH_TIMEOUT(item_->animating(), 2000);
  QSet<QRgb> seen;
  QElapsedTimer clock;
  clock.start();
  while (clock.elapsed() < 1500 && seen.size() < 2) {
    QTest::qWait(10);
    seen.insert(item_->grabImage().pixel(20, 20));
  }
  QVERIFY2(seen.size() >= 2, "the frame changed while playing");
  item_->stopAnimating();
  QVERIFY(!item_->animating());
  // autoplay false: loads paused until startAnimating.
  auto paused = std::make_unique<ExpoImageItem>(window_.contentItem());
  paused->setSize(QSizeF(40, 40));
  paused->setAutoplay(false);
  paused->setSource(sourceOf(path));
  QTRY_VERIFY_WITH_TIMEOUT(paused->displayed(), 3000);
  QVERIFY(!paused->animating());
  paused->startAnimating();
  QVERIFY(paused->animating());
}

void ExpoImageItemTest::hitSlopExtendsAndPointerEventsNoneIgnoresTouches() {
  make(QSizeF(40, 40));
  QVERIFY(!item_->contains(QPointF(-5, 20)));
  item_->setHitSlop(QMarginsF(10, 10, 10, 10));
  QVERIFY(item_->contains(QPointF(-5, 20)));
  QVERIFY(!item_->contains(QPointF(-15, 20)));
  item_->setPointerEvents(QStringLiteral("none"));
  QVERIFY(!item_->contains(QPointF(20, 20)));
  item_->setPointerEvents(QStringLiteral("box-only"));
  QVERIFY(item_->contains(QPointF(20, 20)));
}

void ExpoImageItemTest::downscalingKeepsReportedSize() {
  make(QSizeF(20, 20));
  QSignalSpy loaded(item_.get(), &ExpoImageItem::loaded);
  loadAndWait(sourceOf(writePng(dir_, "huge.png", QSize(400, 400), Qt::red)));
  QCOMPARE(loaded.first().at(1).toDouble(), 400.0);  // onLoad reports the real size
  QVERIFY(near(at(item_->grabImage(), 10, 10), Qt::red));
  // A downscaled decode must not enter the shared cache at reduced size.
  QVERIFY(!DecodedImageCache::shared().contains(QUrl::fromLocalFile(dir_.filePath("huge.png")).toString()));
}

void ExpoImageItemTest::growingADownscaledViewDecodesEnoughPixels() {
  make(QSizeF(20, 20));
  const QString file = writePng(dir_, "grow.png", QSize(400, 400), Qt::red);
  const QString uri = QUrl::fromLocalFile(file).toString();
  QSignalSpy loaded(item_.get(), &ExpoImageItem::loaded);
  loadAndWait(sourceOf(file));
  QCOMPARE(loaded.count(), 1);
  QVERIFY(!DecodedImageCache::shared().contains(uri));
  item_->setSize(QSizeF(240, 240));
  QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 5000);
  QCOMPARE(DecodedImageCache::shared().image(DecodedImageCache::key(uri)).size(), QSize(400, 400));
  QCOMPARE(loaded.last().at(1).toDouble(), 400.0);
  item_->setSize(QSizeF(30, 30));
  QTest::qWait(50);
  QCOMPARE(loaded.count(), 2);
}

void ExpoImageItemTest::lockedGrowthDefersDecodeUntilUnlock() {
  make(QSizeF(20, 20));
  QSignalSpy loaded(item_.get(), &ExpoImageItem::loaded);
  loadAndWait(sourceOf(writePng(dir_, "locked-grow.png", QSize(400, 400), Qt::blue)));
  item_->lockResource();
  item_->setSize(QSizeF(240, 240));
  QTest::qWait(50);
  QCOMPARE(loaded.count(), 1);
  item_->unlockResource();
  QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 5000);
}

void ExpoImageItemTest::upgradingDecodeOptions_data() {
  QTest::addColumn<bool>("naturalFit");
  QTest::newRow("disable-downscaling") << false;
  QTest::newRow("natural-size-fit") << true;
}

void ExpoImageItemTest::upgradingDecodeOptions() {
  QFETCH(bool, naturalFit);
  make(QSizeF(20, 20));
  const QString file = writePng(dir_, "options.png", QSize(400, 400), Qt::green);
  const QString uri = QUrl::fromLocalFile(file).toString();
  QSignalSpy loaded(item_.get(), &ExpoImageItem::loaded);
  loadAndWait(sourceOf(file));
  QVERIFY(!DecodedImageCache::shared().contains(uri));
  if (naturalFit) item_->setContentFit(QStringLiteral("none"));
  else item_->setAllowDownscaling(false);
  QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 5000);
  QCOMPARE(DecodedImageCache::shared().image(DecodedImageCache::key(uri)).size(), QSize(400, 400));
}

void ExpoImageItemTest::growthDuringInitialDecodeIsRechecked() {
  auto* pool = QThreadPool::globalInstance();
  pool->waitForDone();
  const int previousThreads = pool->maxThreadCount();
  pool->setMaxThreadCount(1);
  QSemaphore started, ioGate, decodeStarted, decodeGate;
  const auto cleanup = qScopeGuard([&] {
    ioGate.release();
    decodeGate.release();
    pool->waitForDone();
    pool->setMaxThreadCount(previousThreads);
  });
  pool->start([&] { started.release(); ioGate.acquire(); });
  QVERIFY(started.tryAcquire(1, 1000));
  make(QSizeF(20, 20));
  const QString file = writePng(dir_, "inflight-grow.png", QSize(400, 400), Qt::yellow);
  const QString uri = QUrl::fromLocalFile(file).toString();
  QSignalSpy loading(item_.get(), &ExpoImageItem::loadStarted);
  QSignalSpy loaded(item_.get(), &ExpoImageItem::loaded);
  item_->setSource(sourceOf(file));
  QTRY_COMPARE_WITH_TIMEOUT(loading.count(), 1, 1000);
  // FIFO worker ordering: allow the asynchronous file read, then hold the
  // decoder while the owner-thread fetch completion captures the 20x20 target.
  pool->start([&] { decodeStarted.release(); decodeGate.acquire(); });
  ioGate.release();
  QVERIFY(decodeStarted.tryAcquire(1, 1000));
  QCoreApplication::processEvents();
  QVERIFY(!item_->displayed());
  item_->setSize(QSizeF(240, 240));
  decodeGate.release();
  QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 5000);
  QCOMPARE(DecodedImageCache::shared().image(DecodedImageCache::key(uri)).size(), QSize(400, 400));
}

int main(int argc, char** argv) {
  qputenv("QT_QUICK_BACKEND", "software");
  QGuiApplication app(argc, argv);
  ExpoImageItemTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "ExpoImageItemTest.moc"
