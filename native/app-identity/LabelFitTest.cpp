#include "LabelFit.h"

#include <QTextBoundaryFinder>
#include <QtTest>

using namespace rnqnx::identity;

namespace {

// Marathon Shell launcher label at upstream 567f1fa with Keel's unscaled
// metrics on a 540 px phone: Theme.fontSizeTiny = 20 px and a label width of
// (540 - 2 * 24) / 5 - 6 = 92.4 px, with the 2 px margin prepare-native keeps.
// See docs/getting-started/app-identity.md.
LabelBox marathonBox() {
  static const QList<LauncherFace> faces = registerFontFiles(
      {QStringLiteral(RNQNX_IDENTITY_FONT_DIR "/source-sans-3/SourceSans3-Regular.ttf"),
          QStringLiteral(RNQNX_IDENTITY_FONT_DIR "/source-sans-3/SourceSans3-Light.ttf")});
  return {faces, 20, 92.4, 2};
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
 private Q_SLOTS:
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
  // Golden advances for the bundled fonts, unhinted, so a regression in size,
  // weight or face selection fails instead of agreeing with itself.
  void goldenWidthsPerFace() {
    const auto box = marathonBox();
    QCOMPARE(box.faces[0].weight, 400);
    QCOMPARE(box.faces[1].weight, 300);
    const struct {
      const char* text;
      qreal regular;
      qreal light;
    } golden[] = {{"Messages", 79.34, 77.03}, {"WWWWWWWWWW", 157.19, 153.91},
        {"Field Notes Pro", 125.92, 122.09}, {"Notes", 48.17, 46.94}};
    // Identical on Linux (Qt 6.11.1, FreeType) and macOS (Qt 6.12.0, CoreText)
    // to 0.01 px. "Calculator" is left out: its kerning differs between those
    // hosts (Regular 85.69 vs 85.73, Light 83.72 vs 82.80), more than the
    // tolerance allows. It still fits on both; see upstreamReferenceLabelFits.
    for (const auto& row : golden) {
      const QString text = QString::fromLatin1(row.text);
      const auto regular = measureLabel(text, {{box.faces[0]}, 20, 92.4, 0});
      const auto light = measureLabel(text, {{box.faces[1]}, 20, 92.4, 0});
      QVERIFY2(qAbs(regular.widthPx - row.regular) <= 0.5, qPrintable(text + QString::number(regular.widthPx)));
      QVERIFY2(qAbs(light.widthPx - row.light) <= 0.5, qPrintable(text + QString::number(light.widthPx)));
      // Both faces measured; the wider (Regular) decides.
      const auto both = measureLabel(text, box);
      QCOMPARE(both.widthPx, regular.widthPx);
      QCOMPARE(both.widestFace, box.faces[0].name());
      // Order must not matter: with Light listed first, Regular still decides.
      const auto reversed = measureLabel(text, {{box.faces[1], box.faces[0]}, 20, 92.4, 0});
      QCOMPARE(reversed.widthPx, regular.widthPx);
      QCOMPARE(reversed.widestFace, box.faces[0].name());
    }
    // Half the pixel size is roughly half the width: the size is applied.
    const auto half = measureLabel(QStringLiteral("Messages"), {{box.faces[0]}, 10, 92.4, 0});
    QVERIFY(qAbs(half.widthPx - 79.34 / 2) <= 0.5);
  }
  void fixedFitCounts() {
    // 92.4 px box minus 2 px margin = 90.4 px; Regular W = 15.72 px, i = 4.91 px.
    QCOMPARE(checkBoundary(QString(20, QLatin1Char('W'))), qsizetype(5));
    QCOMPARE(checkBoundary(QString(40, QLatin1Char('i'))), qsizetype(18));
  }
  void marginShrinksTheBox() {
    auto box = marathonBox();
    const QString text = QString(6, QLatin1Char('W')).left(5) + QStringLiteral("ii"); // 88.4 px
    box.marginPx = 0;
    QVERIFY(measureLabel(text, box).fits());
    box.marginPx = 5;
    const auto tight = measureLabel(text, box);
    QVERIFY(!tight.fits());
    QVERIFY(qAbs(tight.overflowPx() - (tight.widthPx - 87.4)) < 0.001);
  }
  void markupIsMeasuredAsPlainTextAndFlagged() {
    const auto markup = measureLabel(QStringLiteral("<b>Notes</b> Pad"), marathonBox());
    QVERIFY(markup.mightBeRichText);
    QVERIFY(!markup.fits()); // The tags count as characters: plain-text width.
    QVERIFY(!markup.suggestion.contains(QLatin1Char('<')));
    QVERIFY(!measureLabel(QStringLiteral("R and D"), marathonBox()).mightBeRichText);
    const auto amp = measureLabel(QStringLiteral("A&B Notes and More"), marathonBox());
    QVERIFY(!amp.suggestion.contains(QLatin1Char('&')));
  }
  // Each face must resolve to its own file. A face whose recorded 'head' table
  // belongs to the other file is refused instead of measured with the wrong font.
  void faceResolvesToItsOwnFile() {
    const auto box = marathonBox();
    QVERIFY(box.faces[0].headTable != box.faces[1].headTable);
    for (const auto& face : box.faces) measureLabel(QStringLiteral("Notes"), {{face}, 20, 92.4, 0});
    LauncherFace mismatched = box.faces[1];
    mismatched.headTable = box.faces[0].headTable;
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, measureLabel(QStringLiteral("Notes"), {{mismatched}, 20, 92.4, 0}));
  }
  void invalidInputsThrow() {
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, measureLabel(QString(), marathonBox()));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, measureLabel(QStringLiteral("a"), LabelBox{{}, 20, 92}));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, measureLabel(QStringLiteral("a"), LabelBox{marathonBox().faces, 0, 92}));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, registerFontFiles({QStringLiteral("/nonexistent/font.ttf")}));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, measureLabel(QStringLiteral("a"), LabelBox{marathonBox().faces, 20, 92, -1}));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, measureLabel(QStringLiteral("a"), LabelBox{marathonBox().faces, 20, 92, 92}));
  }
};

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  LabelFitTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "LabelFitTest.moc"
