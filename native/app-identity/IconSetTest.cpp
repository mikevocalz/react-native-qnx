#include "IconSet.h"
#include "react-native-qnx/runtime/ApplicationIcon.h"

#include <QCryptographicHash>
#include <QFile>
#include <QPainter>
#include <QTemporaryDir>
#include <QtTest>

using namespace rnqnx::identity;

namespace {

QString writeSource(const QTemporaryDir& dir, const QString& name, QSize size, const char* format = "png") {
  QImage image(size, QImage::Format_ARGB32);
  image.fill(QColor(0x29, 0x6e, 0xd8));
  QPainter painter(&image);
  painter.setBrush(Qt::white);
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(QRectF(QPointF(0, 0), QSizeF(size)).adjusted(size.width() / 4.0, size.height() / 4.0,
      -size.width() / 4.0, -size.height() / 4.0));
  painter.end();
  const QString path = dir.filePath(name);
  if (!image.save(path, format)) throw std::runtime_error("fixture");
  return path;
}

QByteArray sha256(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
}

const QList<int> kSizes{16, 24, 32, 48, 64, 96, 128, 192, 256, 512};

} // namespace

class IconSetTest final : public QObject {
  Q_OBJECT
 private slots:
  void writesEverySizeWithSquareCorners() {
    QTemporaryDir dir;
    const auto source = writeSource(dir, QStringLiteral("icon.png"), {1024, 1024});
    const auto icons = writeIconSet(source, dir.filePath(QStringLiteral("out")), kSizes);
    QCOMPARE(icons.size(), kSizes.size());
    for (const auto& icon : icons) {
      const QImage image(icon.path);
      QCOMPARE(image.size(), QSize(icon.size, icon.size));
      // Full-bleed background reaches every corner: nothing masked it.
      QCOMPARE(qAlpha(image.pixel(0, 0)), 255);
      QCOMPARE(qAlpha(image.pixel(icon.size - 1, icon.size - 1)), 255);
    }
  }
  void outputIsDeterministic() {
    QTemporaryDir dir;
    const auto source = writeSource(dir, QStringLiteral("icon.png"), {2048, 2048});
    const auto first = writeIconSet(source, dir.filePath(QStringLiteral("a")), {48, 256});
    const auto second = writeIconSet(source, dir.filePath(QStringLiteral("b")), {48, 256});
    for (qsizetype i = 0; i < first.size(); ++i) QCOMPARE(sha256(first[i].path), sha256(second[i].path));
  }
  void rejectsSmallNonSquareAndNonPng() {
    QTemporaryDir dir;
    const auto out = dir.filePath(QStringLiteral("out"));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,
        writeIconSet(writeSource(dir, QStringLiteral("small.png"), {512, 512}), out, kSizes));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,
        writeIconSet(writeSource(dir, QStringLiteral("wide.png"), {1280, 1024}), out, kSizes));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,
        writeIconSet(writeSource(dir, QStringLiteral("icon.jpg"), {1024, 1024}, "jpg"), out, kSizes));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,
        writeIconSet(writeSource(dir, QStringLiteral("ok.png"), {1024, 1024}), out, {2048}));
    try {
      writeIconSet(writeSource(dir, QStringLiteral("named.png"), {1000, 1000}), out, kSizes);
      QFAIL("expected rejection");
    } catch (const std::runtime_error& error) {
      const QString message = QString::fromUtf8(error.what());
      QVERIFY(message.contains(QStringLiteral("named.png")));
      QVERIFY(message.contains(QStringLiteral("1000x1000")));
      QVERIFY(message.contains(QStringLiteral("1024x1024")));
    }
  }
  void runtimeAppliesTheStagedSet() {
    QTemporaryDir dir;
    const auto source = writeSource(dir, QStringLiteral("icon.png"), {1024, 1024});
    const auto out = dir.filePath(QStringLiteral("icons"));
    writeIconSet(source, out, kSizes);
    const QIcon icon = rnqnx::runtime::loadApplicationIcon(out, kSizes);
    rnqnx::runtime::applyApplicationIcon(icon);
    const auto described = rnqnx::runtime::describeApplicationIconSizes();
    QCOMPARE(described.size(), kSizes.size());
    for (const auto size : kSizes) QVERIFY(described.contains(size));
  }
  void runtimeRejectsPartialSets() {
    QTemporaryDir dir;
    const auto out = dir.filePath(QStringLiteral("icons"));
    writeIconSet(writeSource(dir, QStringLiteral("icon.png"), {1024, 1024}), out, {16, 32});
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, rnqnx::runtime::loadApplicationIcon(out, {16, 32, 48}));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, rnqnx::runtime::loadApplicationIcon(out, {}));
  }
};

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  IconSetTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "IconSetTest.moc"
