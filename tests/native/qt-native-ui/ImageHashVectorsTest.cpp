// BlurHash and ThumbHash against the reference implementations. The vectors
// come from blurhash@2.0.5 (woltapp/blurhash) and thumbhash@0.1.1
// (evanw/thumbhash), run over fixed images; see "producedBy" in
// tests/fixtures/image-hashes/reference-vectors.json.
#include "react-native-qnx/views/image/BlurHash.h"
#include "react-native-qnx/views/image/BlurHashEncoder.h"
#include "react-native-qnx/views/image/ThumbHash.h"
#include "react-native-qnx/views/image/ThumbHashEncoder.h"

#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <algorithm>

using namespace rnqnx::views;

namespace {

QImage rgbaImage(int width, int height, const QByteArray& rgba) {
  QImage image(reinterpret_cast<const uchar*>(rgba.constData()), width, height, width * 4,
               QImage::Format_RGBA8888);
  return image.copy();
}

QImage imageNamed(const QJsonObject& images, const QString& name) {
  const QJsonObject entry = images.value(name).toObject();
  return rgbaImage(entry.value(QStringLiteral("width")).toInt(), entry.value(QStringLiteral("height")).toInt(),
                   QByteArray::fromBase64(entry.value(QStringLiteral("rgba")).toString().toLatin1()));
}

// The largest per-channel difference between a decoded image and reference
// RGBA bytes.
int maxDifference(const QImage& decoded, int width, int height, const QByteArray& rgba) {
  const QImage expected = rgbaImage(width, height, rgba);
  if (decoded.size() != expected.size()) return 1000;
  const QImage actual = decoded.convertToFormat(QImage::Format_RGBA8888);
  int worst = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const QColor a = actual.pixelColor(x, y);
      const QColor e = expected.pixelColor(x, y);
      worst = std::max({worst, std::abs(a.red() - e.red()), std::abs(a.green() - e.green()),
                        std::abs(a.blue() - e.blue()), std::abs(a.alpha() - e.alpha())});
    }
  }
  return worst;
}

}  // namespace

class ImageHashVectorsTest : public QObject {
  Q_OBJECT

  QJsonObject vectors_;

 private Q_SLOTS:
  void initTestCase() {
    QFile file(QStringLiteral(RNQNX_HASH_VECTORS));
    QVERIFY2(file.open(QIODevice::ReadOnly), "reference-vectors.json is missing");
    vectors_ = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY(!vectors_.isEmpty());
    QVERIFY(vectors_.value(QStringLiteral("producedBy")).toString().contains(QStringLiteral("blurhash@2.0.5")));
  }

  void blurhashEncodeMatchesTheReference() {
    const QJsonObject images = vectors_.value(QStringLiteral("images")).toObject();
    const QJsonArray cases = vectors_.value(QStringLiteral("blurhashEncode")).toArray();
    QVERIFY(cases.size() >= 16);
    for (const QJsonValue& value : cases) {
      const QJsonObject entry = value.toObject();
      const QString name = entry.value(QStringLiteral("image")).toString();
      const int cx = entry.value(QStringLiteral("componentsX")).toInt();
      const int cy = entry.value(QStringLiteral("componentsY")).toInt();
      QCOMPARE(encodeBlurHash(imageNamed(images, name), cx, cy), entry.value(QStringLiteral("hash")).toString());
    }
  }

  void blurhashEncodeRejectsBadInput() {
    QVERIFY(encodeBlurHash(QImage(), 4, 3).isEmpty());
    QImage image(4, 4, QImage::Format_RGBA8888);
    image.fill(Qt::red);
    QVERIFY(encodeBlurHash(image, 0, 3).isEmpty());
    QVERIFY(encodeBlurHash(image, 4, 10).isEmpty());
  }

  void blurhashDecodeMatchesTheReference() {
    const QJsonArray cases = vectors_.value(QStringLiteral("blurhashDecode")).toArray();
    QVERIFY(cases.size() >= 3);
    for (const QJsonValue& value : cases) {
      const QJsonObject entry = value.toObject();
      const int width = entry.value(QStringLiteral("width")).toInt();
      const int height = entry.value(QStringLiteral("height")).toInt();
      const QImage decoded = decodeBlurHash(entry.value(QStringLiteral("hash")).toString(), width, height);
      QVERIFY(!decoded.isNull());
      const int worst = maxDifference(decoded, width, height,
                                      QByteArray::fromBase64(entry.value(QStringLiteral("rgba")).toString().toLatin1()));
      QVERIFY2(worst <= 1, qPrintable(QStringLiteral("blurhash %1 differs by %2")
                                          .arg(entry.value(QStringLiteral("hash")).toString()).arg(worst)));
    }
  }

  void thumbhashEncodeMatchesTheReference() {
    const QJsonObject images = vectors_.value(QStringLiteral("images")).toObject();
    const QJsonArray cases = vectors_.value(QStringLiteral("thumbhashEncode")).toArray();
    QVERIFY(cases.size() >= 4);
    for (const QJsonValue& value : cases) {
      const QJsonObject entry = value.toObject();
      const QString name = entry.value(QStringLiteral("image")).toString();
      QCOMPARE(QString::fromLatin1(encodeThumbHash(imageNamed(images, name)).toBase64()),
               entry.value(QStringLiteral("hash")).toString());
    }
  }

  void thumbhashDecodeMatchesTheReference() {
    const QJsonArray cases = vectors_.value(QStringLiteral("thumbhashDecode")).toArray();
    QVERIFY(cases.size() >= 5);
    for (const QJsonValue& value : cases) {
      const QJsonObject entry = value.toObject();
      const QImage decoded = decodeThumbHash(entry.value(QStringLiteral("hash")).toString());
      QVERIFY(!decoded.isNull());
      const int worst = maxDifference(decoded, entry.value(QStringLiteral("width")).toInt(),
                                      entry.value(QStringLiteral("height")).toInt(),
                                      QByteArray::fromBase64(entry.value(QStringLiteral("rgba")).toString().toLatin1()));
      QVERIFY2(worst <= 1, qPrintable(QStringLiteral("thumbhash %1 differs by %2")
                                          .arg(entry.value(QStringLiteral("hash")).toString()).arg(worst)));
    }
  }

  void thumbhashEncodeScalesLargeImagesDown() {
    QImage large(300, 150, QImage::Format_RGBA8888);
    large.fill(QColor(30, 160, 90));
    const QByteArray hash = encodeThumbHash(large);
    QVERIFY(!hash.isEmpty());
    const QImage decoded = decodeThumbHash(hash);
    QVERIFY(!decoded.isNull());
    QVERIFY(decoded.width() > decoded.height());
    QVERIFY(encodeThumbHash(QImage()).isEmpty());
  }
};

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  ImageHashVectorsTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "ImageHashVectorsTest.moc"
