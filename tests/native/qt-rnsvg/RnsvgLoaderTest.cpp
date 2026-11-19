// Tests for the fixture loader and the image diff in support/. The fixture
// runner trusts both, so they get their own checks: that the loader produces
// what react-native-svg's extract helpers produce, and that it stays bounded
// on hostile input.
#include "support/ImageDiff.h"
#include "support/SvgDocumentLoader.h"
#include "support/SvgValues.h"
#include "support/XmlDocument.h"

#include <QGuiApplication>
#include <QPainter>
#include <QTest>

#include <cmath>

using namespace rnqnx::views;
using namespace rnqnx::views::rnsvg;
using namespace rnqnx::views::rnsvg::testing;

namespace {

LoadedDocument load(const char* xml, bool css = false) {
  LoadOptions options;
  options.applyStyleSheets = css;
  return loadSvgDocument(QByteArray(xml), options);
}

// The inner <G> the <Svg> root mounts, then its n-th child.
const ElementProps& child(const LoadedDocument& doc, int index) {
  static const ElementProps empty;
  if (!doc.root) return empty;
  const auto roots = doc.root->elementChildren();
  if (roots.isEmpty()) return empty;
  const auto kids = roots.first()->elementChildren();
  return index < kids.size() ? kids[index]->props() : empty;
}

QList<RnsvgElementItem*> childItems(const LoadedDocument& doc, int index) {
  const auto kids = doc.root->elementChildren().first()->elementChildren();
  return kids[index]->elementChildren();
}

}  // namespace

class RnsvgLoaderTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void attributesBeatStyleDeclarations() {
    auto doc = load("<svg><rect style=\"fill:red;stroke:blue\" fill=\"lime\"/></svg>");
    QVERIFY(doc.root);
    const ElementProps& r = child(doc, 0);
    QCOMPARE(r.kind, Kind::Rect);
    QCOMPARE(r.fill.color, QColor(0, 255, 0));  // `lime`: the attribute wins over the style's red
    QCOMPARE(r.stroke.color, QColor(Qt::blue));  // only the style sets stroke
    QVERIFY(r.propList.contains(QStringLiteral("fill")));
    QVERIFY(r.propList.contains(QStringLiteral("stroke")));
  }

  void kebabAndColonNamesAreCamelCased() {
    QCOMPARE(camelCase(QStringLiteral("stroke-width")), QStringLiteral("strokeWidth"));
    QCOMPARE(camelCase(QStringLiteral("xlink:href")), QStringLiteral("xlinkHref"));
    auto doc = load("<svg><use xlink:href=\"#a\" stroke-miterlimit=\"2\"/></svg>");
    QCOMPARE(child(doc, 0).href, QStringLiteral("a"));
    QCOMPARE(child(doc, 0).strokeMiterlimit, 2.0);
  }

  void brushesFollowExtractBrush() {
    QCOMPARE(parseBrush(QStringLiteral("none")).type, Brush::Type::None);
    QCOMPARE(parseBrush(QStringLiteral("currentColor")).type, Brush::Type::CurrentColor);
    const Brush ref = parseBrush(QStringLiteral("url(#g1)"));
    QCOMPARE(ref.type, Brush::Type::Ref);
    QCOMPARE(ref.ref, QStringLiteral("g1"));
    QCOMPARE(parseBrush(QStringLiteral("rgb(100%, 0%, 50%)")).color, QColor(255, 0, 128));
    QCOMPARE(parseBrush(QStringLiteral("#f008")).color, QColor(255, 0, 0, 136));
    QCOMPARE(parseBrush(QStringLiteral("not-a-color")).type, Brush::Type::None);
    QCOMPARE(parseBrush(QStringLiteral("rgba(0,0,0,0.5)")).color.alpha(), 128);
  }

  void fillDefaultsToBlackWithoutJoiningThePropList() {
    auto doc = load("<svg><path d=\"M0 0L1 1\"/></svg>");
    const ElementProps& p = child(doc, 0);
    QCOMPARE(p.fill.color, QColor(Qt::black));
    QVERIFY(!p.propList.contains(QStringLiteral("fill")));
  }

  void dasharrayOddListsAreDoubled() {
    auto doc = load("<svg><path d=\"M0 0L9 9\" stroke-dasharray=\"5 3 1\" stroke-dashoffset=\"2\"/></svg>");
    const ElementProps& p = child(doc, 0);
    QCOMPARE(int(p.strokeDasharray.size()), 6);
    QCOMPARE(p.strokeDasharray[3].value, 5.0);
    QCOMPARE(p.strokeDashoffset, 2.0);
    auto none = load("<svg><path d=\"M0 0L9 9\" stroke-dasharray=\"none\" stroke-dashoffset=\"2\"/></svg>");
    QVERIFY(child(none, 0).strokeDasharray.empty());
    QCOMPARE(child(none, 0).strokeDashoffset, 0.0);  // an offset without a dash array is null
  }

  void transformStringsCompose() {
    bool ok = false;
    QTransform t = parseTransform(QStringLiteral("translate(10, 20) scale(2)"), &ok);
    QVERIFY(ok);
    QCOMPARE(t.map(QPointF(1, 1)), QPointF(12, 22));  // scale first, then translate
    t = parseTransform(QStringLiteral("translate(0, 280), scale(1, -1)"), &ok);
    QVERIFY(ok);
    QCOMPARE(t.map(QPointF(100, 100)), QPointF(100, 180));
    t = parseTransform(QStringLiteral("rotate(90 10 10)"), &ok);
    QVERIFY(ok);
    QVERIFY(QLineF(t.map(QPointF(20, 10)), QPointF(10, 20)).length() < 1e-9);
    t = parseTransform(QStringLiteral("matrix(1 0 0 1 5 6) skewX(45)"), &ok);
    QVERIFY(ok);
    QCOMPARE(t.map(QPointF(0, 1)), QPointF(6, 7));
    parseTransform(QStringLiteral("scale(1 2 3)"), &ok);
    QVERIFY(!ok);
    parseTransform(QStringLiteral("wobble(1)"), &ok);
    QVERIFY(!ok);
    parseTransform(QStringLiteral("translate(1"), &ok);
    QVERIFY(!ok);
  }

  void invalidTransformBecomesIdentity() {
    auto doc = load("<svg><g transform=\"scale(1 2 3)\"/></svg>");
    QVERIFY(child(doc, 0).matrix.has_value());
    QVERIFY(child(doc, 0).matrix->isIdentity());
  }

  void lengthsHandleUnitsAndGarbage() {
    QCOMPARE(parseLength(QStringLiteral("50%")).unit, Length::Unit::Percent);
    QCOMPARE(parseLength(QStringLiteral("1.5em")).unit, Length::Unit::Em);
    QCOMPARE(parseLength(QStringLiteral("72pt")).value, 96.0);
    QVERIFY(!parseLength(QStringLiteral("abc")).set);
    QVERIFY(!parseLength(QStringLiteral("")).set);
    QVERIFY(!parseLength(QStringLiteral("nan")).set);
    QVERIFY(!parseLength(QStringLiteral("1e999")).set);  // infinite
    QCOMPARE(parseOpacity(QStringLiteral("50%")), 0.5);
    QCOMPARE(parseOpacity(QStringLiteral("7")), 1.0);
    QCOMPARE(parseOpacity(QStringLiteral("-1")), 0.0);
    QCOMPARE(parseOpacity(QStringLiteral("junk")), 1.0);
  }

  void viewBoxParsing() {
    const ParsedViewBox vb = parseViewBox(QStringLiteral("0,0 30 10"), QStringLiteral("xMinYMax slice"));
    QVERIFY(vb.set);
    QCOMPARE(vb.width, 30.0);
    QCOMPARE(vb.align, QStringLiteral("xMinYMax"));
    QCOMPARE(vb.meetOrSlice, 1);
    QVERIFY(!parseViewBox(QStringLiteral("0 0 30"), {}).set);
    QVERIFY(!parseViewBox(QStringLiteral("0 0 x 10"), {}).set);
  }

  void polylinePointsKeepTheUpstreamFirstMinusQuirk() {
    QCOMPARE(polyPointsToPathForTest(QStringLiteral("10,20 30,40"), false), QStringLiteral("M10 20 30 40"));
    QCOMPARE(polyPointsToPathForTest(QStringLiteral("0,0 1,1"), true), QStringLiteral("M0 0 1 1z"));
  }

  void textChildrenFollowExtractText() {
    // Text with one string child gets a TSpan child; a TSpan keeps a lone string as its content.
    auto doc = load("<svg><text>hello</text><text><tspan>a</tspan>mid<tspan>b</tspan></text></svg>");
    QCOMPARE(child(doc, 0).kind, Kind::Text);
    auto kids = childItems(doc, 0);
    QCOMPARE(kids.size(), 1);
    QCOMPARE(kids[0]->props().kind, Kind::TSpan);
    QCOMPARE(kids[0]->props().content, QStringLiteral("hello"));
    kids = childItems(doc, 1);
    QCOMPARE(kids.size(), 3);
    QCOMPARE(kids[0]->props().content, QStringLiteral("a"));
    QCOMPARE(kids[1]->props().content, QStringLiteral("mid"));  // a bare string wrapped in a TSpan
    QCOMPARE(kids[2]->props().content, QStringLiteral("b"));
  }

  void gradientStopsTakeAlphaFromStopOpacity() {
    auto doc = load("<svg><linearGradient id=\"g\">\n"
        "      <stop offset=\"100%\" stop-color=\"red\"/>\n"
        "      <stop offset=\"0\" stop-color=\"rgba(0,0,255,0.2)\" stop-opacity=\"0.5\"/>\n"
        "      <stop offset=\"bogus\" stop-color=\"not-a-color\"/></linearGradient></svg>");
    const ElementProps& g = child(doc, 0);
    QCOMPARE(g.kind, Kind::LinearGradient);
    QCOMPARE(int(g.stops.size()), 2);  // the invalid color is skipped
    QCOMPARE(g.stops[0].offset, 0.0);
    QCOMPARE(g.stops[0].color.alpha(), 128);  // stop-opacity replaces the color's own alpha
    QCOMPARE(g.stops[1].offset, 1.0);
    QCOMPARE(g.x2.value, 100.0);
    QCOMPARE(g.x2.unit, Length::Unit::Percent);
    QVERIFY(child(doc, 0).name == QStringLiteral("g"));
    QCOMPARE(int(childItems(doc, 0).size()), 0);  // stops are not elements
  }

  void radialGradientFallsBackToR() {
    auto doc = load("<svg><radialGradient id=\"r\" r=\"30%\" cx=\"20%\"/></svg>");
    const ElementProps& g = child(doc, 0);
    QCOMPARE(g.rx.value, 30.0);
    QCOMPARE(g.ry.value, 30.0);
    QCOMPARE(g.fx.value, 20.0);  // fx defaults to cx
  }

  void styleSheetsApplyOnlyWhenAsked() {
    const char* xml = "<svg><style>.a{fill:red} #b{fill:lime} rect{stroke:blue}</style>\n"
        "      <rect class=\"a\" id=\"b\" width=\"1\" height=\"1\"/><rect class=\"a\" style=\"fill:yellow\" width=\"1\" height=\"1\"/></svg>";
    auto plain = load(xml, false);
    QVERIFY(!child(plain, 0).propList.contains(QStringLiteral("fill")));
    auto styled = load(xml, true);
    QCOMPARE(child(styled, 0).fill.color, QColor(0, 255, 0));   // the id rule beats the class rule
    QCOMPARE(child(styled, 0).stroke.color, QColor(Qt::blue));  // a type rule adds stroke
    QCOMPARE(child(styled, 1).fill.color, QColor(Qt::yellow));  // an inline declaration stays
  }

  void unknownAndUnmountedTagsAreReported() {
    auto doc = load("<svg><title>t</title><blink/><rect width=\"1\" height=\"1\"/></svg>");
    QVERIFY(doc.root);
    QVERIFY(doc.skipped.contains(QStringLiteral("blink")));
    QVERIFY(!doc.skipped.contains(QStringLiteral("title")));
    QCOMPARE(child(doc, 0).kind, Kind::Rect);
  }

  void rootOpacityColorAndViewBox() {
    auto doc = load("<svg width=\"10\" height=\"20\" viewBox=\"1 2 3 4\" opacity=\"0.25\" color=\"red\" fill=\"blue\"><rect width=\"1\" height=\"1\"/></svg>");
    QVERIFY(doc.root);
    QCOMPARE(doc.rootOpacity, 0.25);
    const RootProps& rp = doc.root->rootProps();
    QCOMPARE(rp.minX, 1.0);
    QCOMPARE(rp.vbHeight, 4.0);
    QCOMPARE(rp.color, QColor(Qt::red));
    QCOMPARE(rp.bbWidth.value, 10.0);
    // <Svg> forwards fill to its inner G, not opacity.
    const auto groups = doc.root->elementChildren();
    QCOMPARE(groups.first()->props().fill.color, QColor(Qt::blue));
    QCOMPARE(groups.first()->props().opacity, 1.0);
  }

  void malformedAndHostileDocuments() {
    QVERIFY(!load("").root);
    QVERIFY(!load("not xml").root);
    QVERIFY(!load("<svg><rect></svg>").root);          // mismatched end tag
    QVERIFY(!load("<div/>").root);                      // not an svg root
    QVERIFY(!load("<svg/><svg/>").root);                // two roots
    // Nesting far beyond the limit is refused, not recursed into.
    QByteArray deep("<svg>");
    for (int i = 0; i < 5000; ++i) deep += "<g>";
    for (int i = 0; i < 5000; ++i) deep += "</g>";
    deep += "</svg>";
    const auto d = loadSvgDocument(deep);
    QVERIFY(!d.root);
    QVERIFY(!d.error.isEmpty());
    // NaN and infinite attribute values do not become geometry.
    auto nan = load("<svg><circle cx=\"NaN\" cy=\"Infinity\" r=\"1e999\"/></svg>");
    QVERIFY(nan.root);
    QVERIFY(!child(nan, 0).cx.set);
    QVERIFY(!child(nan, 0).r.set);
    // Entities and CDATA inside text arrive as one string.
    auto text = load("<svg><text>a &amp; b<![CDATA[ & c]]></text></svg>");
    QCOMPARE(childItems(text, 0)[0]->props().content, QStringLiteral("a & b & c"));
  }

  void manyElementsStayBounded() {
    QByteArray big("<svg>");
    for (int i = 0; i < 20000; ++i) big += "<rect width=\"1\" height=\"1\"/>";
    big += "</svg>";
    const auto doc = loadSvgDocument(big);
    QVERIFY(doc.root);
    QCOMPARE(int(doc.root->elementChildren().first()->elementChildren().size()), 20000);
  }

#if RNQNX_RNSVG_FULL_MODEL
  void markerPatternSymbolProps() {
    auto doc = load("<svg><marker id=\"m\" markerUnits=\"userSpaceOnUse\" refX=\"5\" orient=\"auto\" viewBox=\"0 0 10 10\"/>"
                    "<pattern id=\"p\" width=\".25\" patternUnits=\"userSpaceOnUse\" patternTransform=\"rotate(90)\"/>"
                    "<symbol id=\"s\" viewBox=\"0 0 150 110\" preserveAspectRatio=\"xMinYMin slice\"/>"
                    "<path d=\"M0 0L9 9\" marker=\"url(#m)\" marker-end=\"url(#e)\"/></svg>");
    const ElementProps& m = child(doc, 0);
    QCOMPARE(m.kind, Kind::Marker);
    QVERIFY(!m.markerStrokeWidthUnits);
    QCOMPARE(m.refX.value, 5.0);
    QCOMPARE(m.markerWidth.value, 3.0);  // Marker.tsx default
    QCOMPARE(m.orient, QStringLiteral("auto"));
    QVERIFY(m.viewBox.set);
    const ElementProps& p = child(doc, 1);
    QCOMPARE(p.kind, Kind::Pattern);
    QVERIFY(p.patternUserSpace);
    QVERIFY(p.patternContentUserSpace);  // patternContentUnits defaults to userSpaceOnUse here
    QVERIFY(p.patternTransform.has_value());
    QCOMPARE(p.height.unit, Length::Unit::Percent);  // default 100%
    const ElementProps& sym = child(doc, 2);
    QCOMPARE(sym.kind, Kind::Symbol);
    QCOMPARE(sym.viewBox.align, QStringLiteral("xMinYMin"));
    QCOMPARE(sym.viewBox.meetOrSlice, 1);
    const ElementProps& path = child(doc, 3);
    QCOMPARE(path.markerStart, QStringLiteral("m"));  // the `marker` shorthand
    QCOMPARE(path.markerMid, QStringLiteral("m"));
    QCOMPARE(path.markerEnd, QStringLiteral("e"));    // an explicit markerEnd wins
    QVERIFY(!path.vertices.empty());
  }

  void filterElementsAndDropShadowExpansion() {
    auto doc = load("<svg><filter id=\"f\"><feColorMatrix values=\"1,0 0\n 0.5\"/><feGaussianBlur stdDeviation=\"3 0\"/>"
                    "<feDropShadow dx=\"1\" dy=\"2\" stdDeviation=\"4\" flood-color=\"red\" result=\"r\"/><feTurbulence/></filter></svg>");
    const ElementProps& f = child(doc, 0);
    QCOMPARE(f.kind, Kind::Filter);
    QCOMPARE(f.x.value, -10.0);
    QVERIFY(!f.filterUserSpace);
    QVERIFY(f.primitiveUserSpace);
    const auto prims = childItems(doc, 0);
    QCOMPARE(int(prims.size()), 7);  // matrix, blur, then the five parts of feDropShadow; feTurbulence is unmounted
    namespace fm = rnqnx::views::rnsvg::filter;
    const auto& cm = std::get<fm::ColorMatrix>(prims[0]->props().primitive->params);
    QCOMPARE(int(cm.values.size()), 3);  // split on whitespace, parseFloat of "1,0" reads 1
    const auto& blur = std::get<fm::GaussianBlur>(prims[1]->props().primitive->params);
    QCOMPARE(blur.stdDeviationX, 3.0);
    QCOMPARE(blur.stdDeviationY, 0.0);
    QCOMPARE(prims[2]->props().primitive->in1, QStringLiteral("SourceGraphic"));
    QCOMPARE(prims[3]->props().primitive->result, QStringLiteral("offsetblur"));
    QCOMPARE(prims[6]->props().primitive->result, QStringLiteral("r"));
    QVERIFY(doc.skipped.contains(QStringLiteral("feTurbulence")));
  }

  void textPathProps() {
    auto doc = load("<svg><text><textPath href=\"#p\" startOffset=\"-10%\">a<tspan>b</tspan></textPath></text></svg>");
    const auto kids = childItems(doc, 0);
    QCOMPARE(kids.size(), 1);
    QCOMPARE(kids[0]->props().kind, Kind::TextPath);
    QCOMPARE(kids[0]->props().textPath.href, QStringLiteral("p"));
    QCOMPARE(kids[0]->props().textPath.startOffset.value, -10.0);
    QCOMPARE(kids[0]->elementChildren().size(), 2);
  }
#endif

  void imageDiffMeasuresFailureModes() {
    QImage a(40, 40, QImage::Format_RGB32);
    a.fill(Qt::white);
    QImage b = a;
    DiffMetrics m = compareImages(a, b);
    QVERIFY(m.comparable);
    QCOMPARE(m.differing, qint64(0));
    QCOMPARE(m.largestBlock, 0);
    QVERIFY(checkThresholds(m, DiffThresholds{}).isEmpty());

    // A missing 10x10 shape: one connected block of 100 pixels; only its outer ring could be an edge shift.
    QPainter(&b).fillRect(10, 10, 10, 10, Qt::black);
    m = compareImages(a, b);
    QCOMPARE(m.differing, qint64(100));
    QCOMPARE(m.largestBlock, 100);
    QCOMPARE(m.shiftTolerantBlock, 64);  // the one-pixel ring is forgiven; the 8x8 core is not
    QVERIFY(!checkThresholds(m, DiffThresholds{}).isEmpty());

    // Two separate blocks report the larger one.
    QPainter(&b).fillRect(30, 30, 3, 3, Qt::black);
    QCOMPARE(compareImages(a, b).largestBlock, 100);

    // A one-pixel shift of a thin line is forgiven by the shift-tolerant count but not by the strict one.
    QImage line(20, 20, QImage::Format_RGB32), moved(20, 20, QImage::Format_RGB32);
    line.fill(Qt::white);
    moved.fill(Qt::white);
    for (int y = 0; y < 20; ++y) {
      line.setPixelColor(5, y, Qt::black);
      moved.setPixelColor(6, y, Qt::black);
    }
    m = compareImages(line, moved);
    QCOMPARE(m.differing, qint64(40));
    QCOMPARE(m.shiftTolerantDiffering, qint64(0));
  }

  void imageDiffRefusesBadInput() {
    QVERIFY(!compareImages(QImage(), QImage()).comparable);
    QImage a(10, 10, QImage::Format_RGB32), b(11, 10, QImage::Format_RGB32);
    QVERIFY(!compareImages(a, b).comparable);
    QVERIFY(compareImages(a, b).problem.contains(QStringLiteral("size")));
    // Over the pixel limit: refused before anything is allocated per pixel.
    QImage huge(9000, 9000, QImage::Format_Mono), huge2(9000, 9000, QImage::Format_Mono);
    QVERIFY(!compareImages(huge, huge2).comparable);
    QVERIFY(diffImage(a, b).isNull());
  }

 private:
  static QString polyPointsToPathForTest(const QString& points, bool close);
};

// Declared in SvgElementBuilders.h; included here only for this one helper.
#include "support/SvgElementBuilders.h"
QString RnsvgLoaderTest::polyPointsToPathForTest(const QString& points, bool close) { return polyPointsToPath(points, close); }

QTEST_MAIN(RnsvgLoaderTest)
#include "RnsvgLoaderTest.moc"
