#include "QtSvgItem.h"

#include <QByteArray>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPointer>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <atomic>
#include <stdexcept>
#include <thread>

using namespace rnqnx::views;

namespace {
const QByteArray stripes = R"(<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100" viewBox="10 20 100 50">
  <rect x="10" y="20" width="25" height="50" fill="#ff0000"/>
  <rect x="35" y="20" width="50" height="50" fill="#00ff00"/>
  <rect x="85" y="20" width="25" height="50" fill="#0000ff"/>
</svg>)";

QByteArray solid(const char* color) {
  return QByteArray("<svg viewBox=\"0 0 10 10\"><rect width=\"10\" height=\"10\" fill=\"") + color + "\"/></svg>";
}

QImage render(QtSvgItem& item) {
  QImage image(QSize(qCeil(item.width()), qCeil(item.height())), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  item.paint(&painter);
  painter.end();
  return image;
}
}  // namespace

class SvgDocumentTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void fitModesMetadataAndResize() {
    QtSvgItem item;
    item.setSize(QSizeF(100, 100));
    QSignalSpy loaded(&item, &QtSvgItem::documentLoaded);
    QSignalSpy failed(&item, &QtSvgItem::documentFailed);
    QVERIFY(item.setDocument(stripes));
    QCOMPARE(item.documentState(), SvgDocumentState::Ready);
    QVERIFY(item.documentInfo().has_value());
    QCOMPARE(item.documentInfo()->intrinsicSize, QSizeF(200, 100));
    QCOMPARE(item.documentInfo()->viewBox, QRectF(10, 20, 100, 50));
    QCOMPARE(loaded.count(), 1);
    auto pixels = render(item);
    QCOMPARE(pixels.pixelColor(50, 10).alpha(), 0);
    QCOMPARE(pixels.pixelColor(10, 50), QColor(Qt::red));
    QCOMPARE(pixels.pixelColor(50, 50), QColor(Qt::green));
    QCOMPARE(pixels.pixelColor(90, 50), QColor(Qt::blue));

    item.setResizeMode(SvgResizeMode::Cover);
    pixels = render(item);
    QCOMPARE(pixels.pixelColor(10, 10), QColor(Qt::green));
    QCOMPARE(pixels.pixelColor(90, 90), QColor(Qt::green));
    item.setResizeMode(SvgResizeMode::Stretch);
    pixels = render(item);
    QCOMPARE(pixels.pixelColor(10, 10), QColor(Qt::red));
    QCOMPARE(pixels.pixelColor(90, 90), QColor(Qt::blue));
    item.setResizeMode(SvgResizeMode::Contain);
    item.setSize(QSizeF(200, 100));
    pixels = render(item);
    QCOMPARE(pixels.size(), QSize(200, 100));
    QCOMPARE(pixels.pixelColor(20, 10), QColor(Qt::red));
    QCOMPARE(pixels.pixelColor(180, 90), QColor(Qt::blue));
    QCOMPARE(loaded.count(), 1); // Painting and resizing cannot dispatch load events.
    QCOMPARE(failed.count(), 0);
  }

  void replacementClearAndRecovery() {
    QtSvgItem item;
    item.setSize(QSizeF(32, 32));
    QSignalSpy loaded(&item, &QtSvgItem::documentLoaded);
    QSignalSpy failed(&item, &QtSvgItem::documentFailed);
    QSignalSpy cleared(&item, &QtSvgItem::documentCleared);
    QVERIFY(item.setDocument(solid("red")));
    QVERIFY(item.setDocument(solid("red")));
    QCOMPARE(loaded.count(), 1);
    QCOMPARE(render(item).pixelColor(16, 16), QColor(Qt::red));
    QVERIFY(!item.setDocument("<svg>"));
    QCOMPARE(item.documentState(), SvgDocumentState::Error);
    QVERIFY(!item.documentInfo().has_value());
    QVERIFY(!item.errorString().isEmpty());
    QCOMPARE(render(item).pixelColor(16, 16).alpha(), 0);
    QVERIFY(!item.setDocument("<svg>"));
    QCOMPARE(failed.count(), 1);
    QVERIFY(item.setDocument(solid("blue")));
    QVERIFY(item.errorString().isEmpty());
    QCOMPARE(render(item).pixelColor(16, 16), QColor(Qt::blue));
    QVERIFY(item.setDocument({}));
    item.clearDocument();
    QCOMPARE(cleared.count(), 1);
    QCOMPARE(item.documentState(), SvgDocumentState::Empty);
    QVERIFY(!item.documentInfo().has_value());
    QCOMPARE(render(item).pixelColor(16, 16).alpha(), 0);
  }

  void gradientsPathsAndShapes() {
    QtSvgItem item;
    item.setSize(QSizeF(100, 100));
    QVERIFY(item.setDocument(R"SVG(<svg viewBox="0 0 100 100"><defs>
      <linearGradient id="paint" x1="0%" y1="0%" x2="100%" y2="0%">
        <stop offset="0" stop-color="#ff0000"/>
        <stop offset="1" stop-color="#0000ff"/>
      </linearGradient></defs>
      <rect width="100" height="50" fill="url(#paint)"/>
      <g transform="translate(0,50)"><path d="M 0 0 L 50 0 L 25 50 Z" fill="#00ff00"/>
      <circle cx="75" cy="25" r="20" fill="#ffff00"/></g></svg>)SVG"));
    const auto pixels = render(item);
    QVERIFY(pixels.pixelColor(5, 25).red() > 220);
    QVERIFY(pixels.pixelColor(95, 25).blue() > 220);
    QCOMPARE(pixels.pixelColor(25, 70), QColor(Qt::green));
    QCOMPARE(pixels.pixelColor(75, 75), QColor(Qt::yellow));
    QCOMPARE(pixels.pixelColor(99, 99).alpha(), 0);
  }

  void borrowedSourceIsOwnedBeforeReturn() {
    QtSvgItem item;
    item.setSize(QSizeF(16, 16));
    QSignalSpy loaded(&item, &QtSvgItem::documentLoaded);
    {
      QByteArray callerStorage = solid("red");
      const QByteArray borrowed = QByteArray::fromRawData(callerStorage.constData(), callerStorage.size());
      QVERIFY(item.setDocument(borrowed));
      callerStorage.fill('?');
      QCOMPARE(render(item).pixelColor(8, 8), QColor(Qt::red));
    }
    // Both caller arrays have been destroyed. The cached source still equals
    // independently supplied bytes, so this must not reload or read freed data.
    QVERIFY(item.setDocument(solid("red")));
    QCOMPARE(loaded.count(), 1);
    QCOMPARE(render(item).pixelColor(8, 8), QColor(Qt::red));
    {
      const QByteArray callerStorage = solid("blue");
      QVERIFY(item.setDocument(QByteArrayView(callerStorage)));
    }
    QVERIFY(item.setDocument(solid("blue")));
    QCOMPARE(loaded.count(), 2);
    QCOMPARE(render(item).pixelColor(8, 8), QColor(Qt::blue));
  }

  void compactPathNumbersAndArcFlags() {
    QtSvgItem item;
    item.setSize(QSizeF(20, 20));
    // Adjacent arc flags and coordinates are legal SVG path syntax.
    QVERIFY(item.setDocument(R"(<svg viewBox="0 0 20 20"><path d="M10 0 A10 10 0 0120 10 L10 10Z" fill="red"/></svg>)"));
    const auto pixels = render(item);
    QCOMPARE(pixels.pixelColor(15, 5), QColor(Qt::red));
    QCOMPARE(pixels.pixelColor(5, 5).alpha(), 0);
    QVERIFY(item.setDocument(R"(<svg viewBox="0 0 20 20"><path d="M0 0h2e1v20h-20Z" fill="blue"/></svg>)"));
    QCOMPARE(render(item).pixelColor(10, 10), QColor(Qt::blue));
  }

  void unsupportedInput_data() {
    QTest::addColumn<QByteArray>("xml");
    const auto wrap = [](const QByteArray& inner) { return "<svg viewBox=\"0 0 24 24\">" + inner + "</svg>"; };
    QTest::newRow("script") << wrap("<script>alert(1)</script>");
    QTest::newRow("image") << wrap("<image href=\"file:///tmp/private.png\"/>");
    QTest::newRow("external-use") << wrap("<use href=\"https://example.com/a.svg#x\"/>");
    QTest::newRow("animation") << wrap("<animate attributeName=\"opacity\"/>");
    QTest::newRow("style-block") << wrap("<style>rect { fill: red }</style>");
    QTest::newRow("style-attribute") << wrap("<rect width=\"24\" height=\"24\" style=\"fill:red\"/>");
    QTest::newRow("clip-path") << wrap("<clipPath id=\"clip\"><rect width=\"10\" height=\"10\"/></clipPath>");
    QTest::newRow("filter") << wrap("<filter id=\"blur\"/>");
    QTest::newRow("text") << wrap("<text x=\"0\" y=\"10\">Hello</text>");
    QTest::newRow("external-paint") << wrap("<rect width=\"24\" height=\"24\" fill=\"url(https://example.com/a.svg#red)\"/>");
    QTest::newRow("missing-gradient") << wrap("<rect width=\"24\" height=\"24\" fill=\"url(#missing)\"/>");
    QTest::newRow("wrong-reference-kind") << wrap("<rect id=\"shape\" width=\"12\" height=\"24\"/><circle r=\"3\" fill=\"url(#shape)\"/>");
    QTest::newRow("duplicate-id") << wrap("<rect id=\"x\"/><circle id=\"x\"/>");
    QTest::newRow("custom-namespace") << wrap("<custom:path xmlns:custom=\"urn:custom\"/>");
    QTest::newRow("event-handler") << wrap("<rect onload=\"steal()\"/>");
    QTest::newRow("nested-root") << wrap("<svg viewBox=\"0 0 1 1\"/>");
    QTest::newRow("nonfinite") << QByteArray("<svg width=\"NaN\" height=\"24\"/>");
    QTest::newRow("overflow") << QByteArray("<svg viewBox=\"0 0 1e999 10\"/>");
    QTest::newRow("huge-dimension") << QByteArray("<svg viewBox=\"0 0 16385 10\"/>");
    QTest::newRow("zero-dimension") << QByteArray("<svg width=\"0\" height=\"10\"/>");
    QTest::newRow("percentage-size") << QByteArray("<svg width=\"100%\" height=\"100%\"/>");
    QTest::newRow("unknown-attribute") << wrap("<rect nonsense=\"1\"/>");
    QTest::newRow("invalid-path") << wrap("<path d=\"not a path\"/>");
    QTest::newRow("incomplete-path") << wrap("<path d=\"M 0 0 L\"/>");
    QTest::newRow("incomplete-path-pair") << wrap("<path d=\"M 0 0 L 10\"/>");
    QTest::newRow("invalid-arc-flag") << wrap("<path d=\"M 0 0 A 5 5 0 2 0 10 10\"/>");
    QTest::newRow("numbers-after-close") << wrap("<path d=\"M 0 0 Z 10 10\"/>");
    QTest::newRow("negative-radius") << wrap("<circle r=\"-1\"/>");
    QTest::newRow("negative-stroke") << wrap("<rect width=\"10\" height=\"10\" stroke-width=\"-1\"/>");
    QTest::newRow("negative-dash") << wrap("<path d=\"M 0 0 L 10 10\" stroke-dasharray=\"2 -2\"/>");
    QTest::newRow("zero-dashes") << wrap("<path d=\"M 0 0 L 10 10\" stroke-dasharray=\"0 0\"/>");
    QTest::newRow("tiny-viewbox") << QByteArray("<svg viewBox=\"0 0 1e-200 10\"/>");
    QTest::newRow("transform-product") << wrap("<g transform=\"scale(10000) scale(10000)\"><rect width=\"1\" height=\"1\"/></g>");
    QTest::newRow("nested-transform-product") << wrap("<g transform=\"scale(10000)\"><g transform=\"scale(10000)\"><rect width=\"1\" height=\"1\"/></g></g>");
    QTest::newRow("singular-skew") << wrap("<g transform=\"skewX(90)\"><rect width=\"1\" height=\"1\"/></g>");
    QTest::newRow("gradient-as-color") << wrap("<defs><linearGradient id=\"paint\"><stop offset=\"0\" stop-color=\"red\"/></linearGradient></defs><rect width=\"10\" height=\"10\" color=\"url(#paint)\"/>");
    QTest::newRow("gradient-as-stop-color") << wrap("<defs><linearGradient id=\"paint\"><stop offset=\"0\" stop-color=\"url(#paint)\"/></linearGradient></defs>");
    QTest::newRow("huge-path-number") << wrap("<path d=\"M 0 0 L 1e100 5\"/>");
    QTest::newRow("broken-xml") << QByteArray("<svg viewBox=\"0 0 10 10\"><path></svg>");
    QTest::newRow("no-size") << QByteArray("<svg/>");
    QTest::newRow("doctype") << QByteArray("<!DOCTYPE svg [<!ENTITY secret SYSTEM \"file:///etc/passwd\">]><svg viewBox=\"0 0 10 10\"><desc>&secret;</desc></svg>");
    QTest::newRow("processing-instruction") << QByteArray("<?xml-stylesheet href=\"file:///tmp/a.css\"?><svg viewBox=\"0 0 1 1\"/>");
    QTest::newRow("invalid-utf8") << QByteArray("\xC3\x28", 2);
    QTest::newRow("nul") << QByteArray("<svg\0/>", 7);
  }

  void unsupportedInput() {
    QFETCH(QByteArray, xml);
    QtSvgItem item;
    item.setSize(QSizeF(8, 8));
    QVERIFY(item.setDocument(solid("red")));
    QSignalSpy failed(&item, &QtSvgItem::documentFailed);
    QVERIFY2(!item.setDocument(xml), xml.constData());
    QCOMPARE(item.documentState(), SvgDocumentState::Error);
    QVERIFY(!item.errorString().isEmpty());
    QCOMPARE(failed.count(), 1);
    QCOMPARE(render(item).pixelColor(4, 4).alpha(), 0);
  }

  void boundedInputAndTexture() {
    QtSvgItem item;
    item.setSize(QSizeF(32, 32));
    QVERIFY(item.setDocument(solid("red")));
    QCOMPARE(render(item).pixelColor(16, 16), QColor(Qt::red));
    const QByteArray oversized(kSvgMaxDocumentBytes + 1, ' ');
    QVERIFY(!item.setDocument(QByteArrayView(oversized)));
    QVERIFY(item.errorString().contains("65536"));
    QCOMPARE(item.documentState(), SvgDocumentState::Error);
    QCOMPARE(render(item).pixelColor(16, 16).alpha(), 0);
    QCOMPARE(item.textureSize(), QSize(1, 1));
    QByteArray elements("<svg viewBox=\"0 0 10 10\">");
    for (int i = 0; i < kSvgMaxElements; ++i) elements += "<rect/>";
    QVERIFY(!item.setDocument(elements + "</svg>"));
    QVERIFY(item.errorString().contains("512"));
    QByteArray nesting("<svg viewBox=\"0 0 10 10\">");
    for (int i = 0; i < kSvgMaxDepth; ++i) nesting += "<g>";
    for (int i = 0; i < kSvgMaxDepth; ++i) nesting += "</g>";
    QVERIFY(!item.setDocument(nesting + "</svg>"));
    QVERIFY(item.errorString().contains("32"));
    QVERIFY(item.setDocument(solid("blue")));
    item.setSize(QSizeF(1000000, 500000));
    QVERIFY(item.textureSize().width() <= kSvgMaxTexturePixels);
    QVERIFY(item.textureSize().height() <= kSvgMaxTexturePixels);
    QCOMPARE(item.textureSize().width(), item.textureSize().height() * 2);
    item.setSize(QSizeF(32, 32));
    QCOMPARE(render(item).pixelColor(16, 16), QColor(Qt::blue));
  }

  void ownerThreadAndLifetime() {
    auto parent = std::make_unique<QQuickItem>();
    auto* item = new QtSvgItem(parent.get());
    QPointer<QtSvgItem> weak(item);
    item->setSize(QSizeF(8, 8));
    for (int i = 0; i < 100; ++i) {
      QVERIFY(item->setDocument(solid(i % 2 ? "red" : "blue")));
      item->clearDocument();
      item->clearDocument();
    }
    std::atomic_bool rejected{false};
    std::thread other([&] {
      try { item->setDocument(solid("red")); }
      catch (const std::logic_error&) { rejected = true; }
    });
    other.join();
    QVERIFY(rejected.load());
    QVERIFY(item->setDocument(solid("blue")));
    parent.reset();
    QVERIFY(weak.isNull());
    QCoreApplication::processEvents(); // No queued callbacks own a dead view.
  }

  void workerConstructionAndChangedAffinityAreRejected() {
    std::atomic_bool constructionRejected{false};
    std::thread constructor([&] {
      try { QtSvgItem forbidden; }
      catch (const std::logic_error&) { constructionRejected = true; }
    });
    constructor.join();
    QVERIFY(constructionRejected.load());

    QtSvgItem item;
    item.setSize(QSizeF(8, 8));
    QVERIFY(item.setDocument(solid("red")));
    QThread worker;
    worker.start();
    QVERIFY(item.moveToThread(&worker));
    bool mutationRejected = false;
    auto* owner = QThread::currentThread();
    // This is an adversarial ownership test; production never moves views or
    // blocks the GUI on a worker. Return affinity for deterministic teardown.
    QVERIFY(QMetaObject::invokeMethod(&item, [&] {
      try { item.setDocument(solid("blue")); }
      catch (const std::logic_error&) { mutationRejected = true; }
      item.moveToThread(owner);
    }, Qt::BlockingQueuedConnection));
    worker.quit();
    QVERIFY(worker.wait(2000));
    QVERIFY(mutationRejected);
    QVERIFY_THROWS_EXCEPTION(std::logic_error, item.setDocument(solid("blue")));
    QCOMPARE(render(item).pixelColor(4, 4).alpha(), 0);
  }

  void actualQuickSceneRendersAndClears() {
    QQuickWindow window;
    window.setColor(Qt::white);
    window.resize(120, 100);
    auto* item = new QtSvgItem(window.contentItem());
    item->setPosition(QPointF(10, 20));
    item->setSize(QSizeF(80, 60));
    item->setResizeMode(SvgResizeMode::Stretch);
    QVERIFY(item->setDocument(solid("blue")));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const auto centerColor = [&window] {
      const QImage image = window.grabWindow();
      return image.isNull() ? QColor() : image.pixelColor(image.width() * 50 / 120, image.height() / 2);
    };
    QTRY_COMPARE_WITH_TIMEOUT(centerColor(), QColor(Qt::blue), 2000);
    QVERIFY(item->setDocument(solid("red")));
    QTRY_COMPARE_WITH_TIMEOUT(centerColor(), QColor(Qt::red), 2000);
    QVERIFY(!item->setDocument("<svg>"));
    QTRY_COMPARE_WITH_TIMEOUT(centerColor(), QColor(Qt::white), 2000);
    QVERIFY(item->setDocument(solid("blue")));
    QTRY_COMPARE_WITH_TIMEOUT(centerColor(), QColor(Qt::blue), 2000);
    item->clearDocument();
    QTRY_COMPARE_WITH_TIMEOUT(centerColor(), QColor(Qt::white), 2000);
  }
};

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  SvgDocumentTest tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "SvgDocumentTest.moc"
