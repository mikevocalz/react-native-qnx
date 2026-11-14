// Tests for the SVG text layout engine (native/qt/rnsvg/text). The engine is a
// pure function from a tree of resolved text nodes to glyph geometry, so most
// tests assert on geometry that follows from the font's own advances: glyph
// origins, rotations, advance boxes and decoration rectangles. A second group
// compares the filled outlines with resvg 0.50.0 renders of the SVG sources in
// golden/text/ where resvg and react-native-svg agree.
#include "text/TextLayout.h"
#include "text/TextParse.h"

#include <QFile>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTest>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace rnqnx::views::rnsvg::text;

namespace {

const QString kSans = QStringLiteral("DejaVu Sans");
const QString kMono = QStringLiteral("DejaVu Sans Mono");

Font fontOf(double size, const QString& family = kSans) {
  Font f;
  f.family = family;
  f.size = size;
  return f;
}

Node node(int id, const QString& content, double size = 20) {
  Node n;
  n.id = id;
  n.content = content;
  n.font = fontOf(size);
  return n;
}

// The <text> root: no content of its own, children carry the characters.
Node root(double size = 20) { return node(0, QString(), size); }

std::vector<std::optional<double>> list(std::initializer_list<double> v) {
  std::vector<std::optional<double>> out;
  for (double d : v) out.push_back(d);
  return out;
}

// Advance of one character from Qt's own metrics at the engine's probe size,
// independent of the engine's shaping code.
double advance(QChar c, double size, const QString& family = kSans, int weight = 400) {
  QFont f(family);
  f.setPixelSize(100);
  f.setWeight(QFont::Weight(weight));
  f.setHintingPreference(QFont::PreferNoHinting);
  f.setKerning(false);
  return QFontMetricsF(f).horizontalAdvance(c) * size / 100;
}

double advance(const QString& s, double size, const QString& family = kSans, int weight = 400) {
  double sum = 0;
  for (QChar c : s) sum += advance(c, size, family, weight);
  return sum;
}

double ascent(double size, const QString& family = kSans) {
  QFont f(family);
  f.setPixelSize(100);
  f.setHintingPreference(QFont::PreferNoHinting);
  return QFontMetricsF(f).ascent() * size / 100;
}

constexpr double kTol = 1e-6;

const NodeOutput& out(const Result& r, int id) {
  const NodeOutput* n = r.find(id);
  Q_ASSERT(n);
  return *n;
}

// ---- golden images -------------------------------------------------------

// Why the tolerances are what they are. Qt and resvg place every glyph within
// 0.07 px of each other (compared at 16x zoom: bounding boxes agree to 1/16 px,
// areas to 0.5%), so what remains is anti-aliasing: tiny-skia samples four
// sub-scanlines per pixel, QPainter's raster engine computes exact coverage,
// and edge pixels differ by up to a quarter of full scale. Each limit is the
// measured value on this machine (Qt 6.12 offscreen, resvg 0.50.0) with about
// 40% headroom for rasteriser differences between Qt builds. A shift of 0.25 px
// already doubles the figures and fails them (see
// goldenToleranceRejectsAShiftedLayout); a wrong advance, baseline or rotation
// fails by a wide margin. Measured: basic 0.62% / 0.20, anchor-spans 0.51% /
// 0.16, lists 0.10% / 0.07, spacing 0.24% / 0.08, textlength-spacing 0.36% /
// 0.12, textlength-glyphs 0.60% / 0.17, textpath 0.86% / 0.30, baseline
// 0.26% / 0.09, decoration 2.00% / 0.57, fonts 1.24% / 0.38, bidi 0.69% / 0.21
// (share of pixels differing by more than 12/255 in any channel, then the mean
// absolute difference over all channels, 0..255). The decoration case runs
// highest because its long horizontal edges sit at fractional rows, where the
// two rasterisers quantise coverage differently.
struct GoldenCase {
  QString name;  // golden/text/<name>.svg and .png
  Node tree;
  QPointF start;
  double maxDifferingShare;
  double maxMeanDifference;
};

Node spanOf(int id, const QString& text, double size, Anchor anchor = Anchor::Start) {
  Node n = node(id, text, size);
  n.anchor = anchor;
  return n;
}

QPainterPath circleArc() {
  QPainterPath p;
  p.moveTo(20, 150);
  p.cubicTo(100, 20, 300, 20, 380, 150);
  return p;
}

std::vector<GoldenCase> goldenCases() {
  std::vector<GoldenCase> cases;
  const auto add = [&](const QString& name, Node tree, QPointF start, double share, double mean) {
    cases.push_back({name, std::move(tree), start, share, mean});
  };

  add(QStringLiteral("basic"), node(1, QStringLiteral("Hello World"), 40), QPointF(20, 60), 0.0090, 0.30);

  {
    Node t = root(36);
    t.x = list({200});
    Node bold = spanOf(2, QStringLiteral("dle"), 36, Anchor::Middle);
    bold.font.weight = 700;
    t.children = {spanOf(1, QStringLiteral("Mid"), 36, Anchor::Middle), bold,
                  spanOf(3, QStringLiteral(" text"), 36, Anchor::Middle)};
    t.anchor = Anchor::Middle;
    add(QStringLiteral("anchor-spans"), t, QPointF(0, 80), 0.0070, 0.25);
  }
  {
    Node n = node(1, QStringLiteral("ABCDE"), 44);
    n.dx = list({0, 10, 0, 20});
    n.dy = list({0, -14, 12, 0});
    n.rotate = {0, 15, -15, 30};
    add(QStringLiteral("lists"), n, QPointF(20, 90), 0.0015, 0.11);
  }
  {
    Node n = node(1, QStringLiteral("a b c"), 40);
    n.font.letterSpacing = 6;
    n.font.wordSpacing = 20;
    add(QStringLiteral("spacing"), n, QPointF(20, 70), 0.0035, 0.12);
  }
  {
    Node n = node(1, QStringLiteral("Stretch"), 36);
    n.textLength = 300;
    add(QStringLiteral("textlength-spacing"), n, QPointF(20, 70), 0.0050, 0.18);
    n.lengthAdjust = LengthAdjust::SpacingAndGlyphs;
    add(QStringLiteral("textlength-glyphs"), n, QPointF(20, 70), 0.0085, 0.25);
  }
  {
    Node n = node(1, QStringLiteral("Text along a curve"), 28);
    TextPathSpec spec;
    spec.path = circleArc();
    spec.startOffset = {10, false};
    n.textPath = spec;
    add(QStringLiteral("textpath"), n, QPointF(0, 0), 0.0120, 0.45);
  }
  {
    Node t = root(40);
    Node sup = spanOf(2, QStringLiteral("2"), 24);
    sup.baselineShift = ShiftKeyword::Super;
    Node sub = spanOf(4, QStringLiteral("x"), 24);
    sub.baselineShift = ShiftKeyword::Sub;
    Node num = spanOf(5, QStringLiteral("y"), 40);
    num.baselineShift = 10.0;
    t.children = {spanOf(1, QStringLiteral("H"), 40), sup, spanOf(3, QStringLiteral("O"), 40), sub, num};
    add(QStringLiteral("baseline"), t, QPointF(20, 100), 0.0040, 0.14);
  }
  {
    Node t = root(32);
    const struct { const char* text; unsigned flag; double y; int id; } rows[] = {
        {"Underline", DecorationUnderline, 45, 1}, {"Overline", DecorationOverline, 100, 2},
        {"Line through", DecorationLineThrough, 155, 3}};
    for (const auto& row : rows) {
      Node n = node(row.id, QString::fromLatin1(row.text), 32);
      n.font.decoration = row.flag;
      n.x = list({20});
      n.y = list({row.y});
      t.children.push_back(n);
    }
    add(QStringLiteral("decoration"), t, QPointF(0, 0), 0.0280, 0.85);
  }
  {
    Node t = root(32);
    Node serif = node(1, QStringLiteral("Serif text"), 32);
    serif.font = fontOf(32, QStringLiteral("DejaVu Serif"));
    serif.x = list({20});
    serif.y = list({50});
    Node mono = node(2, QStringLiteral("Mono text"), 32);
    mono.font = fontOf(32, kMono);
    mono.x = list({20});
    mono.y = list({100});
    Node bold = node(3, QStringLiteral("Bold text"), 32);
    bold.font.weight = 700;
    bold.x = list({20});
    bold.y = list({150});
    t.children = {serif, mono, bold};
    add(QStringLiteral("fonts"), t, QPointF(0, 0), 0.0175, 0.57);
  }
  add(QStringLiteral("bidi"), node(1, QString::fromUtf8("ab \xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d \xd7\xa2\xd7\x95\xd7\x9c\xd7\x9d"), 40),
      QPointF(20, 70), 0.0100, 0.31);
  return cases;
}

QImage renderResult(const Result& r, double scale = 1) {
  QImage image(int(400 * scale), int(200 * scale), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::white);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.scale(scale, scale);
  for (const NodeOutput& n : r.nodes) {
    painter.fillPath(n.glyphs, Qt::black);
    painter.fillPath(n.decorations, Qt::black);
  }
  return image;
}

struct ImageDiff {
  double differingShare{0};
  double meanDifference{0};
};

ImageDiff compare(const QImage& a, const QImage& b) {
  const QImage x = a.convertToFormat(QImage::Format_ARGB32);
  const QImage y = b.convertToFormat(QImage::Format_ARGB32);
  qint64 differing = 0, sum = 0;
  const qint64 pixels = qint64(x.width()) * x.height();
  for (int row = 0; row < x.height(); ++row) {
    const QRgb* pa = reinterpret_cast<const QRgb*>(x.constScanLine(row));
    const QRgb* pb = reinterpret_cast<const QRgb*>(y.constScanLine(row));
    for (int col = 0; col < x.width(); ++col) {
      const int dr = std::abs(qRed(pa[col]) - qRed(pb[col]));
      const int dg = std::abs(qGreen(pa[col]) - qGreen(pb[col]));
      const int db = std::abs(qBlue(pa[col]) - qBlue(pb[col]));
      sum += dr + dg + db;
      if (std::max({dr, dg, db}) > 12) ++differing;
    }
  }
  return {double(differing) / double(pixels), double(sum) / double(pixels * 3)};
}

}  // namespace

class RnsvgTextTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void initTestCase() {
    const QString dir = QStringLiteral(RNQNX_RNSVG_TEST_DATA_DIR) + QStringLiteral("/fonts/");
    for (const char* name : {"DejaVuSans.ttf", "DejaVuSans-Bold.ttf", "DejaVuSansMono.ttf", "DejaVuSerif.ttf"}) {
      QVERIFY2(QFontDatabase::addApplicationFont(dir + QLatin1String(name)) >= 0, name);
    }
  }

  // ---- positioning -------------------------------------------------------

  void firstGlyphSitsAtTheStartPosition() {
    Node n = node(1, QStringLiteral("A"), 40);
    const Result r = layout(n, QPointF(50, 100));
    const NodeOutput& o = out(r, 1);
    QCOMPARE(o.placements.size(), size_t(1));
    QVERIFY(qFuzzyCompare(o.placements[0].origin.x(), 50.0));
    QVERIFY(qFuzzyCompare(o.placements[0].origin.y(), 100.0));
    QVERIFY(!o.glyphs.isEmpty());
    // The outline of "A" rises above the baseline by about the cap height.
    QVERIFY(o.glyphs.boundingRect().bottom() <= 100.5);
    QVERIFY(o.glyphs.boundingRect().top() < 100 - 20);
  }

  void advanceBoxCoversTheAdvanceOfTheString() {
    Node n = node(1, QStringLiteral("Hello"), 30);
    const Result r = layout(n, QPointF(10, 50));
    const QRectF box = out(r, 1).advanceBox;
    // No kerning pair in "Hello" for DejaVu Sans, so the box is the plain sum.
    QVERIFY(std::abs(box.width() - advance(QStringLiteral("Hello"), 30)) < 0.01);
    QVERIFY(std::abs(box.left() - 10) < kTol);
    QVERIFY(std::abs(box.top() - (50 - ascent(30))) < 0.01);
  }

  void outputIsGroupedPerNodeInDocumentOrder() {
    Node t = root();
    t.children.push_back(node(7, QStringLiteral("AB")));
    t.children.push_back(node(3, QStringLiteral("CD")));
    const Result r = layout(t, QPointF(0, 30));
    QCOMPARE(r.nodes.size(), size_t(2));
    QCOMPARE(r.nodes[0].id, 7);
    QCOMPARE(r.nodes[1].id, 3);
    QVERIFY(r.find(0) == nullptr);  // the root has no characters of its own
    QVERIFY(r.find(7)->glyphs.boundingRect().right() <= r.find(3)->glyphs.boundingRect().left() + 0.5);
  }

  void anchorAppliesToTheChunkAcrossSpans() {
    Node t = root(20);
    t.x = list({100});
    t.anchor = Anchor::Middle;
    Node a = node(1, QStringLiteral("HH"));
    a.anchor = Anchor::Middle;
    Node b = node(2, QStringLiteral("BBB"));
    b.anchor = Anchor::Middle;
    t.children = {a, b};
    const Result r = layout(t, QPointF(0, 50));
    const double w = advance(QStringLiteral("HH"), 20) + advance(QStringLiteral("BBB"), 20);
    const auto& pa = out(r, 1).placements;
    const auto& pb = out(r, 2).placements;
    QVERIFY(std::abs(pa[0].origin.x() - (100 - w / 2)) < 1e-4);
    // The second span continues the first; it does not centre itself again.
    QVERIFY(std::abs(pb[0].origin.x() - (100 - w / 2 + advance(QStringLiteral("HH"), 20))) < 1e-4);
  }

  void endAnchorUsesTheWholeChunk() {
    Node t = root(20);
    t.x = list({200});
    t.anchor = Anchor::End;
    Node a = node(1, QStringLiteral("ab"));
    a.anchor = Anchor::End;
    t.children = {a};
    const Result r = layout(t, QPointF(0, 50));
    const double w = advance(QStringLiteral("ab"), 20);
    QVERIFY(std::abs(out(r, 1).advanceBox.right() - 200) < 1e-4);
    QVERIFY(std::abs(out(r, 1).placements[0].origin.x() - (200 - w)) < 1e-4);
  }

  void absoluteXStartsANewChunk() {
    Node n = node(1, QStringLiteral("ab"));
    n.anchor = Anchor::End;
    n.x = list({100, 300});
    const Result r = layout(n, QPointF(0, 50));
    const auto& p = out(r, 1).placements;
    QVERIFY(std::abs(p[0].origin.x() - (100 - advance(QLatin1Char('a'), 20))) < 1e-4);
    QVERIFY(std::abs(p[1].origin.x() - (300 - advance(QLatin1Char('b'), 20))) < 1e-4);
  }

  void absoluteYStartsANewChunkAndMovesTheBaseline() {
    Node n = node(1, QStringLiteral("ab"));
    n.y = list({40, 90});
    const Result r = layout(n, QPointF(10, 0));
    const auto& p = out(r, 1).placements;
    QVERIFY(std::abs(p[0].origin.y() - 40) < kTol);
    QVERIFY(std::abs(p[1].origin.y() - 90) < kTol);
    // Only y was given, so b continues on the x line of a ... as a new chunk,
    // which starts where the text position stands.
    QVERIFY(std::abs(p[1].origin.x() - (10 + advance(QLatin1Char('a'), 20))) < 1e-4);
  }

  void rotateListAppliesPerGlyphAndTheLastValueRepeats() {
    Node n = node(1, QStringLiteral("ABC"));
    n.rotate = {10, 20};
    const Result r = layout(n, QPointF(0, 50));
    const auto& p = out(r, 1).placements;
    QCOMPARE(p.size(), size_t(3));
    QVERIFY(std::abs(p[0].rotation - 10) < kTol);
    QVERIFY(std::abs(p[1].rotation - 20) < kTol);
    QVERIFY(std::abs(p[2].rotation - 20) < kTol);
    // Rotation turns each glyph about its own origin; the origins stay on the line.
    QVERIFY(std::abs(p[1].origin.x() - advance(QLatin1Char('A'), 20)) < 1e-4);
    QVERIFY(std::abs(p[1].origin.y() - 50) < kTol);
  }

  void rotatedGlyphTurnsAboutItsOrigin() {
    Node plain = node(1, QStringLiteral("I"), 40);
    Node turned = plain;
    turned.rotate = {90};
    const QRectF a = out(layout(plain, QPointF(100, 100)), 1).glyphs.boundingRect();
    const QRectF b = out(layout(turned, QPointF(100, 100)), 1).glyphs.boundingRect();
    // A vertical bar turns clockwise on screen into a horizontal one that
    // extends right of the origin by the glyph's height.
    QVERIFY(std::abs(b.height() - a.width()) < 0.01);
    QVERIFY(std::abs(b.width() - a.height()) < 0.01);
    QVERIFY(std::abs(b.left() - (200 - a.bottom())) < 0.01);
    QVERIFY(std::abs(b.right() - (200 - a.top())) < 0.01);
  }

  // ---- per-character lists ----------------------------------------------

  void dxAccumulatesAcrossCharacters() {
    Node n = node(1, QStringLiteral("HHH"));
    n.dx = list({5, 10});
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    const double h = advance(QLatin1Char('H'), 20);
    QVERIFY(std::abs(p[0].origin.x() - 5) < 1e-4);
    QVERIFY(std::abs(p[1].origin.x() - (5 + h + 10)) < 1e-4);
    // The third character has no entry: it keeps the shift already applied.
    QVERIFY(std::abs(p[2].origin.x() - (5 + h + 10 + h)) < 1e-4);
  }

  void dyAccumulatesAndYReplaces() {
    Node n = node(1, QStringLiteral("HHH"));
    n.dy = list({3, 4});
    n.y = {std::nullopt, std::nullopt, 200.0};
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    QVERIFY(std::abs(p[0].origin.y() - 53) < kTol);
    QVERIFY(std::abs(p[1].origin.y() - 57) < kTol);
    QVERIFY(std::abs(p[2].origin.y() - 200) < kTol);
  }

  void listIndexCountsAcrossTheWholeSubtree() {
    Node t = root();
    t.dx = list({0, 0, 7});
    t.children = {node(1, QStringLiteral("HH")), node(2, QStringLiteral("H"))};
    const Result r = layout(t, QPointF(0, 50));
    const double h = advance(QLatin1Char('H'), 20);
    QVERIFY(std::abs(out(r, 2).placements[0].origin.x() - (2 * h + 7)) < 1e-4);
  }

  void aSpansOwnListBeatsItsParents() {
    Node t = root();
    t.dx = list({0, 0, 7});
    Node second = node(2, QStringLiteral("H"));
    second.dx = list({20});
    t.children = {node(1, QStringLiteral("HH")), second};
    const Result r = layout(t, QPointF(0, 50));
    const double h = advance(QLatin1Char('H'), 20);
    QVERIFY(std::abs(out(r, 2).placements[0].origin.x() - (2 * h + 20)) < 1e-4);
  }

  void rotateInASpanDoesNotLeakToItsSiblings() {
    Node t = root();
    Node a = node(1, QStringLiteral("HH"));
    a.rotate = {30};
    t.children = {a, node(2, QStringLiteral("H"))};
    const Result r = layout(t, QPointF(0, 50));
    QVERIFY(std::abs(out(r, 1).placements[1].rotation - 30) < kTol);
    QVERIFY(std::abs(out(r, 2).placements[0].rotation) < kTol);
  }

  void nonFiniteListEntriesLeaveTheCharacterUnchanged() {
    Node n = node(1, QStringLiteral("HH"));
    n.x = {std::nan(""), 50.0};
    n.dx = {std::numeric_limits<double>::infinity()};
    n.rotate = {std::nan("")};
    const auto p = out(layout(n, QPointF(10, 50)), 1).placements;
    QVERIFY(std::abs(p[0].origin.x() - 10) < kTol);
    QVERIFY(std::abs(p[1].origin.x() - 50) < kTol);
    QVERIFY(std::abs(p[0].rotation) < kTol);
  }

  // ---- font properties --------------------------------------------------

  void letterSpacingAddsAfterEveryGlyph() {
    Node n = node(1, QStringLiteral("HHH"));
    n.font.letterSpacing = 5;
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    const double h = advance(QLatin1Char('H'), 20);
    QVERIFY(std::abs(p[1].origin.x() - (h + 5)) < 1e-4);
    QVERIFY(std::abs(p[2].origin.x() - 2 * (h + 5)) < 1e-4);
  }

  void wordSpacingWidensOnlySpaces() {
    Node a = node(1, QStringLiteral("H H"));
    Node b = a;
    b.font.wordSpacing = 10;
    const auto pa = out(layout(a, QPointF(0, 50)), 1).placements;
    const auto pb = out(layout(b, QPointF(0, 50)), 1).placements;
    QCOMPARE(pa.size(), size_t(2));  // the space draws nothing
    QVERIFY(std::abs((pb[1].origin.x() - pa[1].origin.x()) - 10) < 1e-4);
    Node c = node(1, QStringLiteral("HH"));
    c.font.wordSpacing = 10;
    QVERIFY(std::abs(out(layout(c, QPointF(0, 50)), 1).placements[1].origin.x() -
                     advance(QLatin1Char('H'), 20)) < 1e-4);
  }

  void autoKerningUsesTheFontsPairs() {
    Node n = node(1, QStringLiteral("AV"), 40);
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    QFont f(kSans);
    f.setPixelSize(100);
    f.setHintingPreference(QFont::PreferNoHinting);
    const QFontMetricsF fm(f);
    const double kerned = (fm.horizontalAdvance(QStringLiteral("AV")) - fm.horizontalAdvance(QLatin1Char('V'))) * 0.4;
    QVERIFY(std::abs(p[1].origin.x() - kerned) < 0.02);
    QVERIFY(p[1].origin.x() < advance(QLatin1Char('A'), 40) - 1);
  }

  void numericKerningReplacesFontKerningAndPrecedesEachGlyph() {
    Node n = node(1, QStringLiteral("AV"), 40);
    n.font.kerning = 3.0;
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    // react-native-svg adds the kerning value before every glyph, the first too.
    QVERIFY(std::abs(p[0].origin.x() - 3) < 1e-4);
    QVERIFY(std::abs(p[1].origin.x() - (3 + advance(QLatin1Char('A'), 40) + 3)) < 1e-4);
    n.font.kerning = 0.0;
    const auto q = out(layout(n, QPointF(0, 50)), 1).placements;
    QVERIFY(std::abs(q[1].origin.x() - advance(QLatin1Char('A'), 40)) < 1e-4);
  }

  void ligaturesFormAndSwitchOff() {
    Node n = node(1, QStringLiteral("fi"), 40);
    QCOMPARE(out(layout(n, QPointF(0, 50)), 1).placements.size(), size_t(1));
    n.font.ligatures = false;
    QCOMPARE(out(layout(n, QPointF(0, 50)), 1).placements.size(), size_t(2));
    n.font.ligatures = true;
    n.font.letterSpacing = 1;  // optional ligatures give way to letter-spacing
    QCOMPARE(out(layout(n, QPointF(0, 50)), 1).placements.size(), size_t(2));
    n.font.letterSpacing = 0;
    n.font.features = {{QStringLiteral("liga"), 0}};
    QCOMPARE(out(layout(n, QPointF(0, 50)), 1).placements.size(), size_t(2));
  }

  void aLigatureBreaksWhenItsSecondCharacterIsPositioned() {
    Node n = node(1, QStringLiteral("fi"), 40);
    n.dx = list({0, 6});
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    QCOMPARE(p.size(), size_t(2));
    QVERIFY(std::abs(p[1].origin.x() - (advance(QLatin1Char('f'), 40) + 6)) < 0.5);
  }

  void fontStretchScalesAdvancesAndOutlines() {
    Node n = node(1, QStringLiteral("HHH"), 40);
    const Result normal = layout(n, QPointF(0, 50));
    n.font.stretchPercent = 75;
    const Result narrow = layout(n, QPointF(0, 50));
    const double ratio = out(narrow, 1).advanceBox.width() / out(normal, 1).advanceBox.width();
    QVERIFY2(std::abs(ratio - 0.75) < 0.01, qPrintable(QString::number(ratio)));
    const double outlineRatio = out(narrow, 1).glyphs.boundingRect().width() / out(normal, 1).glyphs.boundingRect().width();
    QVERIFY2(std::abs(outlineRatio - 0.75) < 0.02, qPrintable(QString::number(outlineRatio)));
  }

  void weightPicksTheBoldFace() {
    Node n = node(1, QStringLiteral("HHH"), 40);
    n.font.weight = 700;
    const double w = out(layout(n, QPointF(0, 50)), 1).advanceBox.width();
    QVERIFY(std::abs(w - advance(QStringLiteral("HHH"), 40, kSans, 700)) < 0.02);
    QVERIFY(w > advance(QStringLiteral("HHH"), 40) + 1);
  }

  void aBoldRequestWithoutABoldFaceThickensTheOutlines() {
    // Whether the mono family has a bold face depends on the host's installed
    // fonts (the bundled files carry none; many Linux systems add one). Both
    // outcomes are checked: with a real bold face the engine uses it as is, and
    // without one it thickens the regular outlines and keeps the regular advance.
    const QStringList styles = QFontDatabase::styles(kMono);
    const bool hasBoldFace = std::any_of(styles.begin(), styles.end(), [](const QString& s) {
      return s.contains(QStringLiteral("Bold"), Qt::CaseInsensitive);
    });
    Node n = node(1, QStringLiteral("l"), 40);
    n.font = fontOf(40, kMono);
    const Result regular = layout(n, QPointF(0, 50));
    n.font.weight = 700;
    const Result bold = layout(n, QPointF(0, 50));
    const double grow = out(bold, 1).glyphs.boundingRect().width() - out(regular, 1).glyphs.boundingRect().width();
    if (hasBoldFace) {
      QVERIFY(std::abs(out(bold, 1).advanceBox.width() - advance(QStringLiteral("l"), 40, kMono, 700)) < 0.02);
      QVERIFY2(grow >= -0.01, qPrintable(QString::number(grow)));
    } else {
      QVERIFY2(grow > 1.2 && grow < 2.2, qPrintable(QString::number(grow)));  // em/24 is 1.67 at size 40
      QVERIFY(std::abs(out(bold, 1).advanceBox.width() - out(regular, 1).advanceBox.width()) < 1e-6);
    }
    // A family with a real bold face is not thickened twice.
    Node sans = node(1, QStringLiteral("H"), 40);
    sans.font.weight = 700;
    const QRectF real = out(layout(sans, QPointF(0, 50)), 1).glyphs.boundingRect();
    QVERIFY(real.width() < out(layout(node(1, QStringLiteral("H"), 40), QPointF(0, 50)), 1).glyphs.boundingRect().width() + 5);
  }

  void italicSlantsTheOutlines() {
    Node n = node(1, QStringLiteral("l"), 40);
    const QRectF upright = out(layout(n, QPointF(0, 50)), 1).glyphs.boundingRect();
    n.font.italic = true;
    const QRectF slanted = out(layout(n, QPointF(0, 50)), 1).glyphs.boundingRect();
    QVERIFY(slanted.width() > upright.width() + 3);
    QVERIFY(std::abs(slanted.height() - upright.height()) < 0.1);
  }

  void monospaceFamilyGivesEqualAdvances() {
    Node n = node(1, QStringLiteral("iWi"), 30);
    n.font = fontOf(30, kMono);
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    const double a = advance(QLatin1Char('i'), 30, kMono);
    QVERIFY(std::abs(p[1].origin.x() - a) < 1e-4);
    QVERIFY(std::abs(p[2].origin.x() - 2 * a) < 1e-4);
  }

  // ---- textLength -------------------------------------------------------

  void textLengthSpacingHitsTheRequestedAdvanceExactly() {
    Node n = node(1, QStringLiteral("HIJK"));
    n.textLength = 200;
    const auto p = out(layout(n, QPointF(10, 50)), 1).placements;
    const double total = p.back().origin.x() + p.back().advance - p[0].origin.x();
    QVERIFY2(std::abs(total - 200) < 1e-6, qPrintable(QString::number(total, 'f', 9)));
    // Spacing leaves the glyph shapes alone.
    const QRectF plain = out(layout(node(1, QStringLiteral("HIJK")), QPointF(10, 50)), 1).glyphs.boundingRect();
    QVERIFY(std::abs(out(layout(n, QPointF(10, 50)), 1).glyphs.boundingRect().height() - plain.height()) < 1e-6);
  }

  void textLengthSpacingAndGlyphsScalesTheGlyphs() {
    Node n = node(1, QStringLiteral("HIJK"));
    const double natural = out(layout(n, QPointF(10, 50)), 1).advanceBox.width();
    n.textLength = 200;
    n.lengthAdjust = LengthAdjust::SpacingAndGlyphs;
    const Result r = layout(n, QPointF(10, 50));
    const auto& p = out(r, 1).placements;
    QVERIFY(std::abs(p.back().origin.x() + p.back().advance - p[0].origin.x() - 200) < 1e-6);
    const double s = 200 / natural;
    QVERIFY(std::abs(p[0].advance - advance(QLatin1Char('H'), 20) * s) < 1e-6);
    const double plainWidth = out(layout(node(1, QStringLiteral("H")), QPointF(0, 50)), 1).glyphs.boundingRect().width();
    Node h = node(1, QStringLiteral("H"));
    h.textLength = 80;
    h.lengthAdjust = LengthAdjust::SpacingAndGlyphs;
    QVERIFY(std::abs(out(layout(h, QPointF(0, 50)), 1).glyphs.boundingRect().width() -
                     plainWidth * 80 / advance(QLatin1Char('H'), 20)) < 1e-4);
  }

  void textLengthOnTheRootCoversItsSpans() {
    Node t = root();
    t.textLength = 300;
    t.children = {node(1, QStringLiteral("HH")), node(2, QStringLiteral("II"))};
    const Result r = layout(t, QPointF(0, 50));
    const auto& first = out(r, 1).placements.front();
    const auto& last = out(r, 2).placements.back();
    QVERIFY(std::abs(last.origin.x() + last.advance - first.origin.x() - 300) < 1e-6);
  }

  void aNestedTextLengthGovernsItsOwnText() {
    Node t = root();
    t.textLength = 400;
    Node inner = node(2, QStringLiteral("II"));
    inner.textLength = 100;
    t.children = {node(1, QStringLiteral("HH")), inner};
    const Result r = layout(t, QPointF(0, 50));
    const auto& in = out(r, 2).placements;
    QVERIFY(std::abs(in.back().origin.x() + in.back().advance - in.front().origin.x() - 100) < 1e-6);
    const auto& a = out(r, 1).placements;
    const auto& last = out(r, 2).placements.back();
    QVERIFY(std::abs(last.origin.x() + last.advance - a.front().origin.x() - 400) < 1e-6);
  }

  void invalidTextLengthIsIgnored() {
    Node n = node(1, QStringLiteral("HIJK"));
    const double natural = out(layout(n, QPointF(0, 50)), 1).advanceBox.width();
    for (double bad : {-5.0, std::nan(""), std::numeric_limits<double>::infinity()}) {
      n.textLength = bad;
      const Result r = layout(n, QPointF(0, 50));
      QVERIFY(std::abs(out(r, 1).advanceBox.width() - natural) < 1e-6);
    }
  }

  // ---- baselines --------------------------------------------------------

  void numericBaselineShiftRaisesTheText() {
    Node n = node(1, QStringLiteral("H"));
    n.baselineShift = 10.0;
    QVERIFY(std::abs(out(layout(n, QPointF(0, 100)), 1).placements[0].origin.y() - 90) < kTol);
    n.baselineShift = -4.0;
    QVERIFY(std::abs(out(layout(n, QPointF(0, 100)), 1).placements[0].origin.y() - 104) < kTol);
  }

  void subAndSuperUseTheFontsOs2Offsets() {
    Node n = node(1, QStringLiteral("H"), 40);
    n.baselineShift = ShiftKeyword::Sub;
    // DejaVu Sans: ySubscriptYOffset 286 and ySuperscriptYOffset 983 in 2048 units.
    QVERIFY(std::abs(out(layout(n, QPointF(0, 100)), 1).placements[0].origin.y() - (100 + 40.0 * 286 / 2048)) < 1e-4);
    n.baselineShift = ShiftKeyword::Super;
    QVERIFY(std::abs(out(layout(n, QPointF(0, 100)), 1).placements[0].origin.y() - (100 - 40.0 * 983 / 2048)) < 1e-4);
    n.baselineShift = ShiftKeyword::Baseline;
    QVERIFY(std::abs(out(layout(n, QPointF(0, 100)), 1).placements[0].origin.y() - 100) < kTol);
  }

  void alignmentBaselineMovesTheBoxByFontMetrics() {
    QFont f(kSans);
    f.setPixelSize(100);
    f.setHintingPreference(QFont::PreferNoHinting);
    const QFontMetricsF fm(f);
    const double size = 40, k = size / 100;
    const double asc = fm.ascent() * k, desc = fm.descent() * k, xh = fm.xHeight() * k;
    struct Case { AlignmentBaseline baseline; double shift; };
    const Case cases[] = {
        {AlignmentBaseline::Baseline, 0},         {AlignmentBaseline::Alphabetic, 0},
        {AlignmentBaseline::Middle, xh / 2},      {AlignmentBaseline::Central, (asc - desc) / 2},
        {AlignmentBaseline::Mathematical, asc / 2}, {AlignmentBaseline::Hanging, 0.8 * asc},
        {AlignmentBaseline::TextTop, asc},        {AlignmentBaseline::TextBottom, -desc},
        {AlignmentBaseline::Ideographic, -desc},
    };
    for (const Case& c : cases) {
      Node n = node(1, QStringLiteral("H"), size);
      n.alignmentBaseline = c.baseline;
      const double y = out(layout(n, QPointF(0, 100)), 1).placements[0].origin.y();
      QVERIFY2(std::abs(y - (100 + c.shift)) < 0.01, qPrintable(QString::number(int(c.baseline))));
    }
  }

  void topAndBottomAlignmentIgnoreBaselineShift() {
    Node n = node(1, QStringLiteral("H"), 40);
    n.alignmentBaseline = AlignmentBaseline::Top;
    n.baselineShift = 25.0;
    const double withShift = out(layout(n, QPointF(0, 100)), 1).placements[0].origin.y();
    n.baselineShift = std::monostate{};
    QVERIFY(std::abs(withShift - out(layout(n, QPointF(0, 100)), 1).placements[0].origin.y()) < kTol);
  }

  void verticalAlignFillsUnsetFieldsAndSpansInheritBaselines() {
    Node a = node(1, QStringLiteral("H"), 40);
    a.verticalAlign = QStringLiteral("10 middle");
    Node b = node(1, QStringLiteral("H"), 40);
    b.baselineShift = 10.0;
    b.alignmentBaseline = AlignmentBaseline::Middle;
    QVERIFY(std::abs(out(layout(a, QPointF(0, 100)), 1).placements[0].origin.y() -
                     out(layout(b, QPointF(0, 100)), 1).placements[0].origin.y()) < kTol);
    // A span without its own values takes its parent's, once (no doubling).
    Node t = root(40);
    t.baselineShift = 6.0;
    t.children = {node(1, QStringLiteral("H"), 40)};
    QVERIFY(std::abs(out(layout(t, QPointF(0, 100)), 1).placements[0].origin.y() - 94) < kTol);
    // A lone token that is not an alignment keyword is a shift, in em here.
    Node c = node(1, QStringLiteral("H"), 40);
    c.verticalAlign = QStringLiteral("0.5em");
    QVERIFY(std::abs(out(layout(c, QPointF(0, 100)), 1).placements[0].origin.y() - 80) < kTol);
  }

  // ---- decorations ------------------------------------------------------

  void underlineFollowsTheFontsPostTable() {
    Node n = node(1, QStringLiteral("Hi H"), 40);
    n.font.decoration = DecorationUnderline;
    const Result r = layout(n, QPointF(10, 100));
    const QRectF line = out(r, 1).decorations.boundingRect();
    // post.underlinePosition -40 (the stroke's centre) and underlineThickness
    // 90, in 2048 units.
    QVERIFY(std::abs(line.center().y() - (100 + 40.0 * 40 / 2048)) < 1e-4);
    QVERIFY(std::abs(line.height() - 40.0 * 90 / 2048) < 1e-4);
    // The line covers the whole advance, spaces included.
    QVERIFY(std::abs(line.left() - 10) < 1e-6);
    QVERIFY(std::abs(line.width() - out(r, 1).advanceBox.width()) < 1e-4);
  }

  void overlineAndLineThroughPositions() {
    Node n = node(1, QStringLiteral("HH"), 40);
    n.font.decoration = DecorationOverline;
    QRectF over = out(layout(n, QPointF(0, 100)), 1).decorations.boundingRect();
    QVERIFY(std::abs(over.center().y() - (100 - ascent(40))) < 0.01);
    n.font.decoration = DecorationLineThrough;
    QRectF strike = out(layout(n, QPointF(0, 100)), 1).decorations.boundingRect();
    // OS/2.yStrikeoutPosition 530 in 2048 units, with the underline's thickness.
    QVERIFY(std::abs(strike.center().y() - (100 - 40.0 * 530 / 2048)) < 1e-4);
    QVERIFY(std::abs(strike.height() - 40.0 * 90 / 2048) < 1e-4);
  }

  void combinedDecorationsGiveOneRectangleEach() {
    Node n = node(1, QStringLiteral("HH"), 40);
    n.font.decoration = DecorationUnderline | DecorationOverline | DecorationLineThrough;
    const QList<QPolygonF> rects = out(layout(n, QPointF(0, 100)), 1).decorations.toSubpathPolygons(QTransform());
    QCOMPARE(rects.size(), qsizetype(3));
  }

  void decorationFollowsTheShiftedBaselineAndEachSpanOwnsItsLine() {
    Node t = root(40);
    Node a = node(1, QStringLiteral("HH"), 40);
    a.font.decoration = DecorationUnderline;
    Node b = node(2, QStringLiteral("HH"), 40);
    b.baselineShift = 10.0;
    t.children = {a, b};
    const Result r = layout(t, QPointF(0, 100));
    QVERIFY(!out(r, 1).decorations.isEmpty());
    QVERIFY(out(r, 2).decorations.isEmpty());
    Node c = a;
    c.baselineShift = 10.0;
    QVERIFY(std::abs(out(layout(c, QPointF(0, 100)), 1).decorations.boundingRect().center().y() -
                     (90 + 40.0 * 40 / 2048)) < 1e-4);
  }

  void decorationOnAVerticalTextPathRunsAlongThePath() {
    Node n = node(1, QStringLiteral("HHHH"), 20);
    n.font.decoration = DecorationUnderline;
    TextPathSpec spec;
    spec.path.moveTo(50, 0);
    spec.path.lineTo(50, 300);
    n.textPath = spec;
    const QRectF line = out(layout(n, QPointF(0, 0)), 1).decorations.boundingRect();
    QVERIFY(line.height() > 3 * advance(QLatin1Char('H'), 20));
    QVERIFY(line.width() < 3);
  }

  // ---- inlineSize -------------------------------------------------------

  void inlineSizeWrapsAtSpaces() {
    Node n = node(1, QStringLiteral("aaa bbb ccc ddd"));
    const double w = advance(QStringLiteral("aaa bbb"), 20) + 1;
    n.inlineSize = w;
    const auto p = out(layout(n, QPointF(10, 40)), 1).placements;
    QFont f(kSans);
    f.setPixelSize(100);
    const QFontMetricsF fm(f);
    const double lineHeight = (fm.ascent() + fm.descent() + fm.leading()) * 0.2;
    QCOMPARE(p.size(), size_t(12));
    QVERIFY(std::abs(p[0].origin.y() - 40) < kTol);
    QVERIFY(std::abs(p[5].origin.y() - 40) < kTol);          // last b of line 1
    QVERIFY(std::abs(p[6].origin.y() - (40 + lineHeight)) < 1e-4);  // first c
    QVERIFY(std::abs(p[6].origin.x() - 10) < 1e-4);
  }

  void inlineSizeAlignsEachLineByTheAnchor() {
    const QString text = QStringLiteral("aaaa bb cc dd");
    const double w = advance(QStringLiteral("aaaa bb"), 20) + 1;
    Node n = node(1, text);
    n.inlineSize = w;
    const double lineTwo = advance(QStringLiteral("cc dd"), 20);
    n.anchor = Anchor::Middle;
    auto p = out(layout(n, QPointF(10, 40)), 1).placements;
    QVERIFY(std::abs(p[6].origin.x() - (10 + (w - lineTwo) / 2)) < 1e-4);
    n.anchor = Anchor::End;
    p = out(layout(n, QPointF(10, 40)), 1).placements;
    QVERIFY(std::abs(p[6].origin.x() - (10 + w - lineTwo)) < 1e-4);
  }

  void inlineSizeKeepsAnOverlongWordWhole() {
    Node n = node(1, QStringLiteral("aa bbbbbbbbbb cc"));
    n.inlineSize = 20.0;
    const auto p = out(layout(n, QPointF(0, 40)), 1).placements;
    QCOMPARE(p.size(), size_t(14));
    QVERIFY(p[2].origin.y() > p[0].origin.y());   // the long word starts a line
    QVERIFY(std::abs(p[11].origin.y() - p[2].origin.y()) < kTol);  // and stays whole
    QVERIFY(p[12].origin.y() > p[11].origin.y());  // "cc" follows below it
  }

  // ---- textPath ---------------------------------------------------------

  static Node onPath(const QString& text, const QPainterPath& path, double size = 20) {
    Node n = node(1, text, size);
    TextPathSpec spec;
    spec.path = path;
    n.textPath = spec;
    return n;
  }

  static QPainterPath line(double x0, double y0, double x1, double y1) {
    QPainterPath p;
    p.moveTo(x0, y0);
    p.lineTo(x1, y1);
    return p;
  }

  // The point a glyph's midpoint lands on, from the origin, rotation and advance.
  static QPointF midpoint(const Placement& p) {
    const double a = p.rotation * M_PI / 180;
    return p.origin + QPointF(std::cos(a), std::sin(a)) * (p.advance / 2);
  }

  void glyphsRunAlongAStraightPath() {
    const Node n = onPath(QStringLiteral("HHHH"), line(0, 100, 300, 100));
    const auto p = out(layout(n, QPointF(0, 0)), 1).placements;
    const double h = advance(QLatin1Char('H'), 20);
    QCOMPARE(p.size(), size_t(4));
    for (size_t k = 0; k < p.size(); ++k) {
      QVERIFY(std::abs(p[k].origin.x() - double(k) * h) < 1e-4);
      QVERIFY(std::abs(p[k].origin.y() - 100) < 1e-6);
      QVERIFY(std::abs(p[k].rotation) < 1e-6);
    }
  }

  void startOffsetInUnitsAndPercent() {
    Node n = onPath(QStringLiteral("HH"), line(0, 100, 300, 100));
    n.textPath->startOffset = {50, false};
    QVERIFY(std::abs(out(layout(n, QPointF()), 1).placements[0].origin.x() - 50) < 1e-4);
    n.textPath->startOffset = {50, true};
    QVERIFY(std::abs(out(layout(n, QPointF()), 1).placements[0].origin.x() - 150) < 1e-4);
  }

  void glyphsBeyondThePathEndAreOmitted() {
    const Node n = onPath(QStringLiteral("HHHHHHHHHH"), line(0, 100, 100, 100));
    const auto p = out(layout(n, QPointF()), 1).placements;
    const double h = advance(QLatin1Char('H'), 20);
    size_t expected = 0;
    while ((double(expected) + 0.5) * h <= 100) ++expected;
    QCOMPARE(p.size(), expected);
    QVERIFY(p.size() < 10);
    for (const Placement& g : p) QVERIFY(midpoint(g).x() <= 100 + 1e-6);
  }

  void glyphsWhoseMidpointIsBeforeThePathStartAreOmitted() {
    Node n = onPath(QStringLiteral("HHHH"), line(0, 100, 300, 100));
    const double h = advance(QLatin1Char('H'), 20);
    n.textPath->startOffset = {-1.2 * h, false};  // the first glyph's midpoint is at -0.7h
    const auto p = out(layout(n, QPointF()), 1).placements;
    QCOMPARE(p.size(), size_t(3));
    QVERIFY(std::abs(midpoint(p[0]).x() - 0.3 * h) < 1e-4);
  }

  void midpointsLieOnADiagonalPathAndRotateWithIt() {
    const Node n = onPath(QStringLiteral("HHHH"), line(0, 0, 300, 300));
    const auto placed = out(layout(n, QPointF()), 1).placements;
    for (const Placement& g : placed) {
      const QPointF m = midpoint(g);
      QVERIFY(std::abs(m.x() - m.y()) < 1e-6);
      QVERIFY(std::abs(g.rotation - 45) < 1e-6);
    }
  }

  void midpointsLieOnACirclePerpendicularToTheRadius() {
    QPainterPath circle;
    circle.addEllipse(QPointF(200, 200), 100, 100);
    const Node n = onPath(QStringLiteral("HHHHHH"), circle);
    const auto p = out(layout(n, QPointF()), 1).placements;
    QCOMPARE(p.size(), size_t(6));
    for (const Placement& g : p) {
      const QPointF m = midpoint(g);
      const QPointF radial = m - QPointF(200, 200);
      QVERIFY2(std::abs(std::hypot(radial.x(), radial.y()) - 100) < 0.05, "midpoint on the circle");
      const double a = g.rotation * M_PI / 180;
      const double cosine = (std::cos(a) * radial.x() + std::sin(a) * radial.y()) / std::hypot(radial.x(), radial.y());
      QVERIFY2(std::abs(cosine) < 0.02, "baseline is tangent to the circle");
    }
  }

  void midpointsLieOnQuadraticAndCubicCurves() {
    QPainterPath quad;
    quad.moveTo(10, 150);
    quad.quadTo(150, -50, 290, 150);
    QPainterPath cubic = circleArc();
    for (const QPainterPath& path : {quad, cubic}) {
      QVector<QPointF> samples;
      for (int i = 0; i <= 4000; ++i) samples.push_back(path.pointAtPercent(i / 4000.0));
      const Node n = onPath(QStringLiteral("Curved text"), path);
      const auto p = out(layout(n, QPointF()), 1).placements;
      QVERIFY(p.size() >= 8);
      for (const Placement& g : p) {
        const QPointF m = midpoint(g);
        double best = 1e9;
        for (const QPointF& q : samples) best = std::min(best, std::hypot(m.x() - q.x(), m.y() - q.y()));
        QVERIFY2(best < 0.12, qPrintable(QString::number(best)));
      }
    }
  }

  void aClosedPathIsTraversedOnce() {
    QPainterPath circle;
    circle.addEllipse(QPointF(100, 100), 20, 20);  // circumference about 125.7
    const Node n = onPath(QStringLiteral("HHHHHHHHHHHHHHHH"), circle);  // 16 glyphs, about 220 long
    const auto p = out(layout(n, QPointF()), 1).placements;
    const double h = advance(QLatin1Char('H'), 20);
    QVERIFY(p.size() < 16);
    QVERIFY(double(p.size()) * h < 2 * M_PI * 20 + h);
  }

  void sideRightReversesThePath() {
    Node n = onPath(QStringLiteral("HH"), line(0, 100, 300, 100));
    n.textPath->side = PathSide::Right;
    const Result r = layout(n, QPointF());
    const auto& p = out(r, 1).placements;
    QVERIFY(std::abs(p[0].origin.x() - 300) < 1e-4);
    QVERIFY(std::abs(std::abs(p[0].rotation) - 180) < 1e-6);
    // Upside down, the glyphs hang below the line.
    QVERIFY(out(r, 1).glyphs.boundingRect().top() >= 100 - 0.5);
  }

  void anchorShiftsTheTextAlongThePath() {
    Node n = onPath(QStringLiteral("HHHH"), line(0, 100, 400, 100));
    n.textPath->startOffset = {200, false};
    const double w = 4 * advance(QLatin1Char('H'), 20);
    n.anchor = Anchor::Middle;
    QVERIFY(std::abs(out(layout(n, QPointF()), 1).placements[0].origin.x() - (200 - w / 2)) < 1e-4);
    n.anchor = Anchor::End;
    QVERIFY(std::abs(out(layout(n, QPointF()), 1).placements[0].origin.x() - (200 - w)) < 1e-4);
  }

  void dyOffsetsAcrossThePath() {
    Node n = onPath(QStringLiteral("HH"), line(0, 100, 300, 100));
    n.dy = list({-5});
    QVERIFY(std::abs(out(layout(n, QPointF()), 1).placements[0].origin.y() - 95) < 1e-6);
  }

  void onlyTheFirstSubpathCarriesText() {
    QPainterPath two;
    two.moveTo(0, 100);
    two.lineTo(100, 100);
    two.moveTo(0, 200);
    two.lineTo(300, 200);
    const Node n = onPath(QStringLiteral("HHHHHHHHHH"), two);
    const auto placed = out(layout(n, QPointF()), 1).placements;
    for (const Placement& g : placed) QVERIFY(std::abs(g.origin.y() - 100) < 1e-6);
  }

  void midLineSmoothTurnsGraduallyAndSharpSnapsAtACorner() {
    QPainterPath corner;
    corner.moveTo(0, 0);
    corner.lineTo(100, 0);
    corner.lineTo(100, 100);
    Node n = onPath(QStringLiteral("H"), corner, 20);
    const double h = advance(QLatin1Char('H'), 20);
    n.textPath->startOffset = {100 - h / 2, false};  // the midpoint sits on the corner
    n.textPath->midLine = PathMidLine::Smooth;
    QVERIFY(std::abs(out(layout(n, QPointF()), 1).placements[0].rotation - 45) < 1e-6);
    n.textPath->midLine = PathMidLine::Sharp;
    QVERIFY(std::abs(out(layout(n, QPointF()), 1).placements[0].rotation - 90) < 1e-6);
  }

  void spacingAutoAndExactPlaceGlyphsAlike() {
    Node n = onPath(QStringLiteral("HHH"), line(0, 100, 300, 100));
    const auto a = out(layout(n, QPointF()), 1).placements;
    n.textPath->spacing = PathSpacing::Exact;
    const auto b = out(layout(n, QPointF()), 1).placements;
    QCOMPARE(a.size(), b.size());
    for (size_t k = 0; k < a.size(); ++k) QVERIFY(std::abs(a[k].origin.x() - b[k].origin.x()) < kTol);
  }

  void stretchBendsGlyphsOntoACurve() {
    // An underscore is a flat bar a little below the baseline. On a circle of
    // radius R every stretched point sits at R plus or minus the bar's own
    // offset from the baseline; a rigidly rotated bar has its ends further out.
    QPainterPath circle;
    circle.addEllipse(QPointF(100, 100), 40, 40);
    Node n = onPath(QStringLiteral("_"), circle, 60);
    const QRectF bar = out(layout(onPath(QStringLiteral("_"), line(0, 100, 300, 100), 60), QPointF()), 1)
                           .glyphs.boundingRect();
    const double d1 = bar.top() - 100, d2 = bar.bottom() - 100;
    const auto deviation = [&](PathMethod method) {
      n.textPath->method = method;
      const QPainterPath g = out(layout(n, QPointF()), 1).glyphs;
      double worst = 0;
      for (int i = 0; i < g.elementCount(); ++i) {
        const double r = std::hypot(g.elementAt(i).x - 100, g.elementAt(i).y - 100);
        double best = 1e9;
        for (double c : {40 - d1, 40 - d2, 40 + d1, 40 + d2}) best = std::min(best, std::abs(r - c));
        worst = std::max(worst, best);
      }
      return worst;
    };
    const double stretch = deviation(PathMethod::Stretch);
    const double align = deviation(PathMethod::Align);
    QVERIFY2(stretch < 0.1, qPrintable(QString::number(stretch)));
    QVERIFY2(align > 1.0, qPrintable(QString::number(align)));
  }

  void stretchKeepsAStraightPathIntact() {
    Node n = onPath(QStringLiteral("HI"), line(0, 100, 300, 100), 30);
    const QRectF align = out(layout(n, QPointF()), 1).glyphs.boundingRect();
    n.textPath->method = PathMethod::Stretch;
    const QRectF stretch = out(layout(n, QPointF()), 1).glyphs.boundingRect();
    QVERIFY(std::abs(align.left() - stretch.left()) < 0.05);
    QVERIFY(std::abs(align.right() - stretch.right()) < 0.05);
    QVERIFY(std::abs(align.top() - stretch.top()) < 0.05);
    QVERIFY(std::abs(align.bottom() - stretch.bottom()) < 0.05);
  }

  // ---- bidirectional text -----------------------------------------------

  void hebrewRunsLayOutRightToLeft() {
    Node n = node(1, QString::fromUtf8("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d"), 30);  // shalom
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    QCOMPARE(p.size(), size_t(4));
    for (size_t k = 1; k < p.size(); ++k) {
      QVERIFY(p[k].origin.x() > p[k - 1].origin.x());
      QVERIFY(p[k].charIndex < p[k - 1].charIndex);  // the first letter is rightmost
    }
  }

  void arabicShapesAndRunsRightToLeft() {
    Node n = node(1, QString::fromUtf8("\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a"), 30);
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    QVERIFY(p.size() >= 4);
    for (size_t k = 1; k < p.size(); ++k) QVERIFY(p[k].charIndex < p[k - 1].charIndex);
    QVERIFY(out(layout(n, QPointF(0, 50)), 1).advanceBox.width() > 30);
  }

  void mixedDirectionKeepsTheLatinRunsInOrder() {
    Node n = node(1, QString::fromUtf8("ab \xd7\xa9\xd7\x9c ab"), 30);
    const auto p = out(layout(n, QPointF(0, 50)), 1).placements;
    QCOMPARE(p.size(), size_t(6));
    QCOMPARE(p[0].charIndex, 0);
    QCOMPARE(p[1].charIndex, 1);
    QCOMPARE(p[2].charIndex, 4);  // the Hebrew pair, reversed: lamed then shin
    QCOMPARE(p[3].charIndex, 3);
    QCOMPARE(p[4].charIndex, 6);
    QCOMPARE(p[5].charIndex, 7);
  }

  // ---- bounds and hostile input -----------------------------------------

  void aHugeStringIsCappedWithAPartialResult() {
    Node n = node(1, QString(30000, QLatin1Char('a')));
    const Result r = layout(n, QPointF(0, 50));
    QVERIFY(r.truncated);
    QVERIFY(int(out(r, 1).placements.size()) <= kMaxGlyphs);
    QVERIFY(!out(r, 1).placements.empty());
  }

  void aDeepTreeIsCut() {
    Node t = root();
    Node* cur = &t;
    for (int i = 0; i < 80; ++i) {
      cur->children.push_back(node(i + 1, QStringLiteral("a")));
      cur = &cur->children.back();
    }
    const Result r = layout(t, QPointF());
    QVERIFY(r.truncated);
    QVERIFY(int(r.nodes.size()) <= kMaxDepth + 1);
    QVERIFY(!r.nodes.empty());
  }

  void aBreadthOfEmptyNodesIsCut() {
    Node t = root();
    for (int i = 0; i < 40000; ++i) t.children.push_back(node(i + 1, QString()));
    const Result r = layout(t, QPointF());
    QVERIFY(r.truncated);
  }

  void manySmallNodesFinishQuickly() {
    Node t = root();
    for (int i = 0; i < 15000; ++i) t.children.push_back(node(i + 1, QStringLiteral("a")));
    QElapsedTimer timer;
    timer.start();
    const Result r = layout(t, QPointF(0, 20));
    QVERIFY2(timer.elapsed() < 10000, qPrintable(QString::number(timer.elapsed()) + " ms"));
    QCOMPARE(r.nodes.size(), size_t(15000));
  }

  void invalidFontSizesDrawNothing() {
    for (double size : {0.0, -12.0, std::nan(""), std::numeric_limits<double>::infinity(), 1e300}) {
      Node n = node(1, QStringLiteral("abc"), size);
      const Result r = layout(n, QPointF(0, 50));
      QVERIFY(r.nodes.empty() || r.nodes[0].glyphs.isEmpty());
    }
  }

  void nonFiniteStartAndSpacingDoNotPoisonTheResult() {
    Node n = node(1, QStringLiteral("HH"));
    n.font.letterSpacing = std::nan("");
    n.font.wordSpacing = std::numeric_limits<double>::infinity();
    n.font.kerning = std::nan("");
    const Result r = layout(n, QPointF(std::nan(""), 50));
    const QRectF box = out(r, 1).advanceBox;
    QVERIFY(std::isfinite(box.left()) && std::isfinite(box.width()));
    QVERIFY(std::abs(box.left()) < kTol);
  }

  void hostilePathsAndOffsetsTerminate() {
    QPainterPath bad;
    bad.moveTo(0, 0);
    bad.lineTo(std::nan(""), 100);
    Node n = onPath(QStringLiteral("HHH"), bad);
    QVERIFY(out(layout(n, QPointF()), 1).glyphs.isEmpty());

    n = onPath(QStringLiteral("HHH"), line(5, 5, 5, 5));  // zero length
    QVERIFY(out(layout(n, QPointF()), 1).glyphs.isEmpty());

    n = onPath(QStringLiteral("HHH"), line(0, 0, 100, 0));
    for (double offset : {1e300, -1e300, std::nan(""), std::numeric_limits<double>::infinity()}) {
      n.textPath->startOffset = {offset, false};
      (void)layout(n, QPointF());
      n.textPath->startOffset = {offset, true};
      (void)layout(n, QPointF());
    }

    QPainterPath huge;
    huge.moveTo(0, 0);
    for (int i = 1; i < 80000; ++i) huge.lineTo(i * 0.01, (i % 7) * 0.5);
    n = onPath(QStringLiteral("HHH"), huge);
    QVERIFY(layout(n, QPointF()).truncated);
  }

  void hostileInlineSizeAndTextLengthTerminate() {
    Node n = node(1, QStringLiteral("a b c d e f"));
    for (double w : {-3.0, 0.0, std::nan(""), 1e-9, 1e300}) {
      n.inlineSize = w;
      (void)layout(n, QPointF(0, 50));
    }
    n.inlineSize.reset();
    n.textLength = 1e12;
    const Result r = layout(n, QPointF(0, 50));
    QVERIFY(!out(r, 1).placements.empty());
  }

  // ---- golden images ----------------------------------------------------

  void matchesResvg_data() {
    QTest::addColumn<int>("index");
    const std::vector<GoldenCase> cases = goldenCases();
    for (size_t i = 0; i < cases.size(); ++i) QTest::newRow(qPrintable(cases[i].name)) << int(i);
  }

  void matchesResvg() {
    QFETCH(int, index);
    const std::vector<GoldenCase> cases = goldenCases();
    const GoldenCase& c = cases[size_t(index)];
    const QString path = QStringLiteral(RNQNX_RNSVG_TEST_DATA_DIR "/golden/text/") + c.name + QStringLiteral(".png");
    QImage golden(path);
    QVERIFY2(!golden.isNull(), qPrintable(path));
    const QImage mine = renderResult(layout(c.tree, c.start));
    const QByteArray dump = qgetenv("RNQNX_RNSVG_TEXT_DUMP");
    if (!dump.isEmpty()) {
      // Debugging aid: also dump an 16x render to compare with `resvg -z 16`.
      mine.save(QString::fromLocal8Bit(dump) + QLatin1Char('/') + c.name + QStringLiteral(".png"));
      renderResult(layout(c.tree, c.start), 16).save(QString::fromLocal8Bit(dump) + QLatin1Char('/') + c.name + QStringLiteral("-16.png"));
    }
    const ImageDiff d = compare(mine, golden);
    qInfo("%s: %.4f%% of pixels differ by more than 12, mean difference %.4f", qPrintable(c.name),
          d.differingShare * 100, d.meanDifference);
    QVERIFY2(d.differingShare <= c.maxDifferingShare, qPrintable(QString::number(d.differingShare * 100) + "%"));
    QVERIFY2(d.meanDifference <= c.maxMeanDifference, qPrintable(QString::number(d.meanDifference)));
  }

  void goldenToleranceRejectsAShiftedLayout() {
    const GoldenCase c = goldenCases().front();  // "basic"
    const QImage golden(QStringLiteral(RNQNX_RNSVG_TEST_DATA_DIR "/golden/text/") + c.name + QStringLiteral(".png"));
    QVERIFY(!golden.isNull());
    const ImageDiff exact = compare(renderResult(layout(c.tree, c.start)), golden);
    QVERIFY(exact.meanDifference <= c.maxMeanDifference);
    const ImageDiff shifted = compare(renderResult(layout(c.tree, c.start + QPointF(0.25, 0))), golden);
    QVERIFY2(shifted.meanDifference > c.maxMeanDifference || shifted.differingShare > c.maxDifferingShare,
             "a 0.25 px shift must exceed the golden tolerance");
    const ImageDiff off = compare(renderResult(layout(c.tree, c.start + QPointF(0, 1))), golden);
    QVERIFY(off.meanDifference > 4 * c.maxMeanDifference);
  }

  // ---- string props -----------------------------------------------------

  void stringPropsMapToTheEngineEnums() {
    QCOMPARE(parseAnchor(QStringLiteral("middle")), Anchor::Middle);
    QCOMPARE(parseAnchor(QStringLiteral("nonsense")), Anchor::Start);
    QCOMPARE(parseLengthAdjust(QStringLiteral("spacingAndGlyphs")), LengthAdjust::SpacingAndGlyphs);
    QCOMPARE(*parseAlignmentBaseline(QStringLiteral("text-before-edge")), AlignmentBaseline::TextTop);
    QCOMPARE(*parseAlignmentBaseline(QStringLiteral("after-edge")), AlignmentBaseline::TextBottom);
    QVERIFY(!parseAlignmentBaseline(QStringLiteral("auto")));
    QVERIFY(std::holds_alternative<ShiftKeyword>(parseBaselineShiftKeyword(QStringLiteral("super"))));
    QVERIFY(std::holds_alternative<std::monostate>(parseBaselineShiftKeyword(QStringLiteral("12"))));
    QCOMPARE(parseDecoration(QStringLiteral("underline line-through")), unsigned(DecorationUnderline | DecorationLineThrough));
    QCOMPARE(parseDecoration(QStringLiteral("none")), unsigned(DecorationNone));
    QCOMPARE(parseStretch(QStringLiteral("condensed")), 75.0);
    QCOMPARE(parseStretch(QStringLiteral("80%")), 80.0);
    QCOMPARE(parseSide(QStringLiteral("right")), PathSide::Right);
    QCOMPARE(parseMethod(QStringLiteral("stretch")), PathMethod::Stretch);
    QCOMPARE(parseSpacing(QStringLiteral("exact")), PathSpacing::Exact);
    QCOMPARE(parseMidLine(QStringLiteral("sharp")), PathMidLine::Sharp);
    const auto features = parseFeatureSettings(QStringLiteral("\"liga\" 0, 'smcp' on, kern=1, bad"));
    QCOMPARE(features.size(), size_t(3));
    QCOMPARE(features[0].value, 0u);
    QCOMPARE(features[1].tag, QStringLiteral("smcp"));
    QCOMPARE(features[2].tag, QStringLiteral("kern"));
  }
};

QTEST_MAIN(RnsvgTextTest)
#include "RnsvgTextTest.moc"
