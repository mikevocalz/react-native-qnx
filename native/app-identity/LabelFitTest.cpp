#include "LabelFit.h"

#include <QTextBoundaryFinder>
#include <QtTest>

using namespace rnqnx::identity;

namespace {

// Marathon Shell launcher label at upstream 567f1fa with Keel's unscaled
// metrics on a 540 px phone: Theme.fontSizeTiny = 20 px and a label width of
// (540 - 2 * 24) / 5 - 6 = 92.4 px. See docs/getting-started/app-identity.md.
LabelBox marathonBox() {
  static const QList<LauncherFace> faces = registerFontFiles(
      {QStringLiteral(RNQNX_IDENTITY_FONT_DIR "/source-sans-3/SourceSans3-Regular.ttf"),
          QStringLiteral(RNQNX_IDENTITY_FONT_DIR "/source-sans-3/SourceSans3-Light.ttf")});
  return {faces, 20, 92.4};
}

QStringList graphemes(const QString& text) {
  QStringList clusters;
  QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
  qsizetype start = 0;
  for (auto end = finder.toNextBoundary(); end > 0; end = finder.toNextBoundary()) {
    clusters.append(text.mid(start, end - start));
    start = end;
  }
  return clusters;
}

// Grows the label one grapheme at a time and returns how many fit. Verifies
// the boundary: the fitting prefix fits and one more cluster overflows by a
// measured, positive amount that is at most that cluster's own advance.
qsizetype checkBoundary(const QString& text) {
  const auto box = marathonBox();
  const auto clusters = graphemes(text);
  QString prefix;
  qsizetype count = 0;
  for (const auto& cluster : clusters) {
    const auto next = measureLabel(prefix + cluster, box);
    if (!next.fits()) {
      const auto fitting = measureLabel(prefix, box);
      const auto alone = measureLabel(cluster, box);
      if (!fitting.fits() || !(next.overflowPx() > 0) ||
          next.overflowPx() > alone.widthPx + 0.5 || next.lineCount != 1) {
        return -1;
      }
      return count;
    }
    prefix += cluster;
    ++count;
  }
  return -2; // The whole string fit; the caller chose too short a sample.
}

} // namespace

class LabelFitTest final : public QObject {
  Q_OBJECT
 private slots:
  void upstreamReferenceLabelFits() {
    // Upstream LauncherGrid.js sizes columns so "Calculator" fits at fontSizeTiny.
    const auto measured = measureLabel(QStringLiteral("Calculator"), marathonBox());
    QVERIFY2(measured.fits(), qPrintable(QString::number(measured.widthPx)));
    QCOMPARE(measured.lineCount, 1);
    QVERIFY(measured.fallbackCodePoints.isEmpty());
  }
  void longestFittingNameAndOneGlyphOver() {
    const auto fitting = checkBoundary(QStringLiteral("Field Notes Professional"));
    QVERIFY2(fitting > 0, qPrintable(QString::number(fitting)));
    const auto text = QStringLiteral("Field Notes Professional").left(fitting);
    QVERIFY(measureLabel(text, marathonBox()).fits());
    const auto over = measureLabel(QStringLiteral("Field Notes Professional").left(fitting + 1), marathonBox());
    QVERIFY(!over.fits());
    QVERIFY(over.overflowPx() > 0);
  }
  void wideGlyphsFitFewerThanNarrowOnes() {
    const auto wide = checkBoundary(QString(40, QLatin1Char('W')));
    const auto narrow = checkBoundary(QString(80, QLatin1Char('i')));
    QVERIFY(wide > 0 && narrow > 0);
    QVERIFY2(wide * 2 < narrow, qPrintable(QStringLiteral("W=%1 i=%2").arg(wide).arg(narrow)));
  }
  void cjkUsesFallbackAndStillMeasures() {
    const QString text = QStringLiteral("学習ノート管理アプリケーション");
    QVERIFY(checkBoundary(text) >= 0);
    const auto measured = measureLabel(text, marathonBox());
    QVERIFY(!measured.fallbackCodePoints.isEmpty());
    QVERIFY(measured.fallbackCodePoints.contains(U'学'));
  }
  void emojiClustersAreOneStep() {
    const QString family = QString::fromUtf8("\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB");
    QCOMPARE(graphemes(family).size(), 1);
    QVERIFY(checkBoundary(family.repeated(20)) >= 0);
    const auto measured = measureLabel(QString::fromUtf8("Notes \xF0\x9F\x93\x9D"), marathonBox());
    QVERIFY(measured.fallbackCodePoints.contains(char32_t(0x1F4DD)));
    QVERIFY(!measured.fallbackCodePoints.contains(char32_t(0x200D)));
  }
  void rightToLeftStaysOneLine() {
    const QString hebrew = QStringLiteral("פתקים ורשימות לעבודה");
    const auto measured = measureLabel(hebrew, marathonBox());
    QVERIFY(measured.rightToLeft);
    QCOMPARE(measured.lineCount, 1);
    QVERIFY(checkBoundary(hebrew) >= 0);
    QVERIFY(checkBoundary(QStringLiteral("مدير الملاحظات اليومية")) >= 0);
  }
  void overflowIsMeasuredNotTruncated() {
    const QString text = QStringLiteral("Field Notes Professional Edition");
    const auto measured = measureLabel(text, marathonBox());
    QVERIFY(!measured.fits());
    QCOMPARE(measured.lineCount, 1);
    QVERIFY(measured.widthPx > measured.boxWidthPx);
    QVERIFY(!measured.suggestion.isEmpty());
    QVERIFY(text.startsWith(measured.suggestion));
    QVERIFY(measureLabel(measured.suggestion, marathonBox()).fits());
  }
  void widestFaceDecides() {
    const auto box = marathonBox();
    const auto both = measureLabel(QStringLiteral("Messages"), box);
    QCOMPARE(box.faces.size(), 2);
    QVERIFY(box.faces[0].weight != box.faces[1].weight);
    qreal widest = 0;
    for (const auto& face : box.faces) {
      const auto one = measureLabel(QStringLiteral("Messages"), {{face}, box.pixelSize, box.widthPx});
      QVERIFY(one.widthPx <= both.widthPx);
      widest = std::max(widest, one.widthPx);
    }
    QCOMPARE(widest, both.widthPx);
  }
  void invalidInputsThrow() {
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, measureLabel(QString(), marathonBox()));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, measureLabel(QStringLiteral("a"), LabelBox{{}, 20, 92}));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, measureLabel(QStringLiteral("a"), LabelBox{marathonBox().faces, 0, 92}));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, registerFontFiles({QStringLiteral("/nonexistent/font.ttf")}));
  }
};

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  LabelFitTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "LabelFitTest.moc"
