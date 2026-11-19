// Runs the react-native-svg fixture corpus (tests/fixtures/rnsvg) through the
// renderer and compares each rendering with the resvg reference image.
//
// Each fixture is an SVG document ported from an upstream react-native-svg
// example. The test-only loader (support/SvgDocumentLoader) builds the retained
// element tree the Fabric bridge would produce for it, the real RnsvgRootItem
// paints that tree at 1x onto white, and support/ImageDiff measures the
// difference from tests/fixtures/rnsvg/golden/<name>.png.
//
// Fixture status in manifest.json:
//   pass                 must be within its thresholds
//   pending:<feature>    the renderer cannot draw it yet; reported as a skip with the feature named
//   reference-none       no reference image exists; the document must load and paint without a crash
//
// Environment:
//   RNQNX_RNSVG_FAIL_ON_PENDING=1  a pending fixture fails instead of skipping (CI sets this once the lead has
//                                  removed every pending entry)
//   RNQNX_RNSVG_RUN_PENDING=1      run pending fixtures too: one that is now within its thresholds prints a "READY" line
//                                  (remove its pending status), one that is not stays a skip with its metrics
//   RNQNX_RNSVG_CALIBRATE=1        print one "CAL" line of metrics per fixture, write <name>.actual.png and
//                                  <name>.diff.png for each, and never fail on the metrics
#include "support/FixtureManifest.h"
#include "support/ImageDiff.h"
#include "support/SvgDocumentLoader.h"

#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPainter>
#include <QTest>

using namespace rnqnx::views;
using namespace rnqnx::views::rnsvg;
using namespace rnqnx::views::rnsvg::testing;

namespace {

QString fixtureDir() { return QDir(QStringLiteral(RNQNX_RNSVG_TEST_DATA_DIR) + QStringLiteral("/../../fixtures/rnsvg")).canonicalPath(); }

// The root item painted at 1x onto opaque white, as the reference was rendered
// (resvg --background white). <svg opacity> composites the whole layer, which
// React Native does for the view.
QImage renderDocument(const LoadedDocument& doc, const QSize& size) {
  QImage layer(size, QImage::Format_ARGB32_Premultiplied);
  layer.fill(Qt::transparent);
  {
    QPainter painter(&layer);
    doc.root->paint(&painter);
  }
  QImage out(size, QImage::Format_RGB32);
  out.fill(Qt::white);
  QPainter painter(&out);
  painter.setOpacity(doc.rootOpacity);
  painter.drawImage(0, 0, layer);
  return out;
}

struct Tally {
  int passed{0}, pending{0}, failed{0}, smoke{0};
};

}  // namespace

class RnsvgFixturesTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void initTestCase() {
    for (const char* font : {"DejaVuSans.ttf", "DejaVuSans-Bold.ttf", "DejaVuSansMono.ttf", "DejaVuSerif.ttf"}) {
      const int id = QFontDatabase::addApplicationFont(QStringLiteral(RNQNX_RNSVG_TEST_DATA_DIR "/fonts/") + QLatin1String(font));
      QVERIFY2(id >= 0, font);
    }
    // resvg is told --font-family "DejaVu Sans" and uses 12 px by default.
    QFont base(QStringLiteral("DejaVu Sans"));
    base.setPixelSize(12);
    QGuiApplication::setFont(base);
    QString error;
    fixtures_ = loadFixtureManifest(fixtureDir(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!fixtures_.empty());
  }

  void cleanupTestCase() {
    qInfo().noquote() << QStringLiteral("rnsvg-fixtures: %1 passed, %2 pending, %3 failed, %4 smoke-only (no reference image)")
                             .arg(tally_.passed).arg(tally_.pending).arg(tally_.failed).arg(tally_.smoke);
  }

  void fixture_data() {
    QTest::addColumn<int>("index");
    // initTestCase runs before the data function, so the manifest is loaded.
    for (size_t i = 0; i < fixtures_.size(); ++i) QTest::newRow(qPrintable(fixtures_[i].name)) << int(i);
  }

  void fixture() {
    QFETCH(int, index);
    const Fixture& fx = fixtures_[size_t(index)];
    const bool calibrate = qEnvironmentVariableIntValue("RNQNX_RNSVG_CALIBRATE") != 0;
    const bool failOnPending = qEnvironmentVariableIntValue("RNQNX_RNSVG_FAIL_ON_PENDING") != 0;
    const bool pending = fx.pending();
    const bool runPending = qEnvironmentVariableIntValue("RNQNX_RNSVG_RUN_PENDING") != 0;
    if (pending && !calibrate && !runPending) {
      ++tally_.pending;
      if (failOnPending) QFAIL(qPrintable(QStringLiteral("pending: %1").arg(fx.pendingFeature())));
      QSKIP(qPrintable(QStringLiteral("pending: %1").arg(fx.pendingFeature())), SkipAll);
    }

    const QByteArray svg = readFileBytes(fx.svgPath);
    QVERIFY2(!svg.isEmpty(), qPrintable(fx.svgPath));

    QImage golden;
    if (!fx.goldenPath.isEmpty()) {
      QVERIFY2(golden.load(fx.goldenPath), qPrintable(fx.goldenPath));
      golden = golden.convertToFormat(QImage::Format_RGB32);
    }
    LoadOptions options;
    options.applyStyleSheets = fx.css;
    options.itemSize = golden.isNull() ? QSizeF(200, 200) : QSizeF(golden.size());
    LoadedDocument doc = loadSvgDocument(svg, options);
    QVERIFY2(doc.root, qPrintable(QStringLiteral("%1: load failed: %2").arg(fx.name, doc.error)));
    const QImage actual = renderDocument(doc, golden.isNull() ? QSize(200, 200) : golden.size());

    if (golden.isNull()) {
      ++tally_.smoke;
      QVERIFY(!actual.isNull());
      return;
    }

    const DiffMetrics m = compareImages(actual, golden, fx.thresholds.channelThreshold);
    if (calibrate) {
      qInfo().noquote() << QStringLiteral("CAL %1 comparable=%2 pct=%3 mean=%4 block=%5 stpct=%6 stblock=%7 skipped=[%8]")
                               .arg(fx.name).arg(m.comparable).arg(m.differingPercent, 0, 'f', 3).arg(m.meanAbsDiff, 0, 'f', 3)
                               .arg(m.largestBlock).arg(m.shiftTolerantPercent, 0, 'f', 3).arg(m.shiftTolerantBlock)
                               .arg(doc.skipped.join(QLatin1Char(',')));
      // Both images are kept so a calibration run can be inspected.
      actual.save(fx.name + QStringLiteral(".actual.png"));
      const QImage all = diffImage(actual, golden, fx.thresholds.channelThreshold);
      if (!all.isNull()) all.save(fx.name + QStringLiteral(".diff.png"));
      return;
    }
    const QString why = checkThresholds(m, fx.thresholds);
    if (pending) {
      // Reaches here only with RNQNX_RNSVG_RUN_PENDING.
      ++tally_.pending;
      if (why.isEmpty()) qInfo().noquote() << QStringLiteral("READY %1 is within its thresholds; remove %2").arg(fx.name, fx.status);
      QSKIP(qPrintable(QStringLiteral("%1: %2").arg(fx.status, why.isEmpty() ? QStringLiteral("within thresholds") : why)), SkipAll);
    }
    if (why.isEmpty()) {
      ++tally_.passed;
      return;
    }
    ++tally_.failed;
    actual.save(fx.name + QStringLiteral(".actual.png"));
    const QImage diff = diffImage(actual, golden, fx.thresholds.channelThreshold);
    if (!diff.isNull()) diff.save(fx.name + QStringLiteral(".diff.png"));
    QFAIL(qPrintable(QStringLiteral("%1: %2 (wrote %1.actual.png and %1.diff.png%3)")
                         .arg(fx.name, why, doc.skipped.isEmpty() ? QString() : QStringLiteral("; elements not mounted: ") + doc.skipped.join(QLatin1Char(',')))));
  }

 private:
  std::vector<Fixture> fixtures_;
  Tally tally_;
};

QTEST_MAIN(RnsvgFixturesTest)
#include "RnsvgFixturesTest.moc"
