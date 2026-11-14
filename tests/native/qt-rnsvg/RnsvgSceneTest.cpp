// Scene-level tests: clip rules, layer sizing, hit testing through clip, mask
// and pointerEvents, patterns, symbols, markers, gradient spread, dashes and
// non-scaling strokes. Same method as RnsvgRenderTest.cpp: build the retained
// tree directly, paint, probe pixels well inside or outside edges.
#include "RnsvgForeignObjectItem.h"
#include "RnsvgTestKit.h"
#include "scene/SceneLayout.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QQuickWindow>
#include <QSignalSpy>

#include <cmath>
#include <limits>

using namespace rnqnx::views;
using namespace rnqnx::views::rnsvg;
using namespace rnqnx::views::rnsvg::testkit;

namespace {

ElementProps definition(Kind kind, const QString& id) {
  ElementProps p = make(kind);
  p.name = id;
  return p;
}

ElementProps stroked(Kind kind, const QColor& c, double width) {
  ElementProps p = make(kind);
  p.fill = Brush{};
  p.stroke = color(c);
  p.strokeWidth = px(width);
  p.propList = {QStringLiteral("fill"), QStringLiteral("stroke"), QStringLiteral("strokeWidth")};
  return p;
}

ElementProps line(double x1, double y1, double x2, double y2, const QColor& c, double width) {
  ElementProps p = stroked(Kind::Line, c, width);
  p.x1 = px(x1); p.y1 = px(y1); p.x2 = px(x2); p.y2 = px(y2);
  return p;
}

// A five-pointed star drawn as one self-crossing polygon: its middle pentagon
// is covered twice, so nonzero fills it and evenodd leaves a hole.
const QString kPentagram = QStringLiteral("M50 5 L80 95 L5 38 L95 38 L20 95 Z");

}  // namespace

class RnsvgSceneTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void vertexRecorderFollowsPathData() {
    std::vector<PathVertex> v;
    parseSvgPath(QStringLiteral("M0 0 L10 0 L10 10 Z"), nullptr, &v);
    QCOMPARE(int(v.size()), 4);
    QVERIFY(v[0].subpathStart);
    QCOMPARE(v[1].point, QPointF(10, 0));
    QCOMPARE(v[1].inDirection, QPointF(1, 0));
    QCOMPARE(v[1].outDirection, QPointF(0, 1));
    QVERIFY(v[3].closesSubpath);
    QCOMPARE(v[3].point, QPointF(0, 0));
    // The closing segment is a vertex of its own; the first vertex keeps no
    // incoming direction (neither renderer bisects a closed start).
    QCOMPARE(v[0].inDirection, QPointF());
    QVERIFY(v[3].inDirection.x() < 0);
    // A path that already ends at its start adds no vertex; the zero-length
    // closing segment leaves the last vertex along (1, 0), as resvg does.
    parseSvgPath(QStringLiteral("M0 0 L10 0 L10 10 L0 0 Z"), nullptr, &v);
    QCOMPARE(int(v.size()), 4);
    QCOMPARE(v[3].outDirection, QPointF(1, 0));
    // An arc is one vertex per cubic it becomes, as react-native-svg's own
    // path parser does: a half circle is two quarter cubics.
    parseSvgPath(QStringLiteral("M0 0 A10 10 0 1 1 20 0"), nullptr, &v);
    QCOMPARE(int(v.size()), 3);
    // A curve's direction at its ends comes from its control points.
    parseSvgPath(QStringLiteral("M0 0 C0 10 10 20 20 20"), nullptr, &v);
    QCOMPARE(v[0].outDirection, QPointF(0, 1));
    QCOMPARE(v[1].inDirection, QPointF(1, 0));
  }

  void clipRuleEvenOddOnTheReferencingElement() {
    for (const int rule : {-1, 0, 1}) {
      Scene s;
      auto* defs = add(&s.root, make(Kind::Defs));
      auto* clip = add(defs, definition(Kind::ClipPath, QStringLiteral("c")));
      ElementProps star = make(Kind::Path);
      star.path = parseSvgPath(kPentagram);
      add(clip, star);
      ElementProps r = rect(0, 0, 100, 100, Qt::red);
      r.clipPath = QStringLiteral("c");
      r.clipRule = rule;
      add(&s.root, r);
      const QImage img = render(s.root);
      VERIFY_PX(img, 50, 10 + 0, Qt::red);  // a tip of the star
      if (rule == 1) {
        VERIFY_PX(img, 50, 50, Qt::red);  // nonzero fills the middle pentagon
      } else {
        VERIFY_TRANSPARENT(img, 50, 50);  // unset clips as evenodd, like upstream: a hole
      }
    }
  }

  void clipRuleOnAShapeInsideTheClipPath() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    auto* clip = add(defs, definition(Kind::ClipPath, QStringLiteral("c")));
    ElementProps star = make(Kind::Path);
    star.path = parseSvgPath(kPentagram);
    star.clipRule = 0;  // the shape names its own rule
    add(clip, star);
    ElementProps r = rect(0, 0, 100, 100, Qt::red);
    r.clipPath = QStringLiteral("c");
    add(&s.root, r);
    const QImage img = render(s.root);
    VERIFY_TRANSPARENT(img, 50, 50);
    VERIFY_PX(img, 50, 10, Qt::red);
  }

  void clipPathOfAClipPathIntersects() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    auto* outer = add(defs, definition(Kind::ClipPath, QStringLiteral("outer")));
    add(outer, rect(0, 0, 60, 100, Qt::black));
    ElementProps innerDef = definition(Kind::ClipPath, QStringLiteral("inner"));
    innerDef.clipPath = QStringLiteral("outer");
    auto* inner = add(defs, innerDef);
    add(inner, rect(0, 0, 100, 50, Qt::black));
    ElementProps r = rect(0, 0, 100, 100, Qt::red);
    r.clipPath = QStringLiteral("inner");
    add(&s.root, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 30, 25, Qt::red);
    VERIFY_TRANSPARENT(img, 80, 25);  // outside outer
    VERIFY_TRANSPARENT(img, 30, 75);  // outside inner
  }

  void groupLayersCoverOnlyTheGroupsExtent() {
    Scene s(2000, 2000);
    ElementProps g = make(Kind::Group);
    g.opacity = 0.5;
    auto* group = add(&s.root, g);
    add(group, rect(1000, 1000, 50, 50, Qt::red));
    const QImage img = render(s.root);
    VERIFY_PX(img, 1025, 1025, QColor(255, 0, 0, 128));
    VERIFY_TRANSPARENT(img, 10, 10);
    const PaintStats stats = s.root.lastPaintStats();
    QCOMPARE(stats.layers, 1);
    // The 50x50 rect plus anti-aliasing margin, not the 2000x2000 texture.
    QVERIFY2(stats.largestLayerPixels < 60 * 60, qPrintable(QString::number(stats.largestLayerPixels)));
  }

  void filterFreeMaskLayerIsBoundedByTheMaskedElement() {
    Scene s(2000, 2000);
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps m = definition(Kind::Mask, QStringLiteral("m"));
    m.maskUserSpace = true;
    m.x = px(0); m.y = px(0); m.width = px(2000); m.height = px(2000);
    auto* mask = add(defs, m);
    add(mask, rect(0, 0, 2000, 2000, Qt::white));
    ElementProps masked = rect(100, 100, 40, 40, Qt::blue);
    masked.mask = QStringLiteral("m");
    add(&s.root, masked);
    const QImage img = render(s.root);
    VERIFY_PX(img, 120, 120, Qt::blue);
    QVERIFY2(s.root.lastPaintStats().largestLayerPixels < 60 * 60,
             qPrintable(QString::number(s.root.lastPaintStats().largestLayerPixels)));
  }

  void hitTestingHonorsClipPath() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    auto* clip = add(defs, definition(Kind::ClipPath, QStringLiteral("c")));
    add(clip, rect(0, 0, 50, 100, Qt::black));
    ElementProps g = make(Kind::Group);
    g.clipPath = QStringLiteral("c");
    auto* group = add(&s.root, g);
    ElementProps r = rect(0, 0, 100, 100, Qt::red);
    r.responsible = true;
    auto* item = add(group, r);
    QCOMPARE(s.root.hitElement(QPointF(25, 50)), item);
    QCOMPARE(s.root.hitElement(QPointF(75, 50)), nullptr);  // clipped away
  }

  void hitTestingHonorsMask() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps m = definition(Kind::Mask, QStringLiteral("m"));
    m.maskUserSpace = true;
    m.x = px(0); m.y = px(0); m.width = px(100); m.height = px(100);
    auto* mask = add(defs, m);
    add(mask, rect(0, 0, 50, 100, Qt::white));  // visible only on the left
    ElementProps r = rect(0, 0, 100, 100, Qt::red);
    r.mask = QStringLiteral("m");
    r.responsible = true;
    auto* item = add(&s.root, r);
    QCOMPARE(s.root.hitElement(QPointF(25, 50)), item);
    QCOMPARE(s.root.hitElement(QPointF(75, 50)), nullptr);
  }

  void pointerEventsSelectsTheHitRegion() {
    auto probe = [](const QString& mode, bool fillNone, const QPointF& at) {
      Scene s;
      ElementProps r = fillNone ? stroked(Kind::Rect, Qt::black, 10) : rect(0, 0, 0, 0, Qt::red);
      r.x = px(20); r.y = px(20); r.width = px(60); r.height = px(60);
      r.responsible = true;
      r.pointerEvents = mode;
      auto* item = add(&s.root, r);
      return s.root.hitElement(at) == item;
    };
    // A stroke-only rect: the middle is empty under the default and "painted".
    QVERIFY(!probe(QString(), true, QPointF(50, 50)));
    QVERIFY(probe(QString(), true, QPointF(20, 50)));          // on the stroke
    QVERIFY(probe(QStringLiteral("visibleFill"), true, QPointF(50, 50)));   // fill area even with fill none
    QVERIFY(!probe(QStringLiteral("visibleFill"), true, QPointF(18, 50)));  // stroke area excluded
    QVERIFY(probe(QStringLiteral("visibleStroke"), true, QPointF(18, 50)));
    QVERIFY(!probe(QStringLiteral("visibleStroke"), true, QPointF(50, 50)));
    QVERIFY(probe(QStringLiteral("all"), true, QPointF(50, 50)));
    QVERIFY(!probe(QStringLiteral("none"), false, QPointF(50, 50)));
    QVERIFY(probe(QStringLiteral("box-only"), true, QPointF(50, 50)));
    QVERIFY(!probe(QStringLiteral("box-only"), true, QPointF(5, 5)));
    QVERIFY(!probe(QStringLiteral("box-none"), false, QPointF(50, 50)));  // the shape itself is skipped
  }

  void boxNoneGroupStillPassesPressesToChildren() {
    Scene s;
    ElementProps g = make(Kind::Group);
    g.responsible = true;
    g.pointerEvents = QStringLiteral("box-none");
    auto* group = add(&s.root, g);
    ElementProps child = rect(0, 0, 50, 50, Qt::red);
    child.responsible = true;
    auto* item = add(group, child);
    QCOMPARE(s.root.hitElement(QPointF(25, 25)), item);
    // The group itself never takes a press under box-none, though it is
    // responsible: a press on a non-responsible child area must miss it.
    QCOMPARE(s.root.hitElement(QPointF(75, 75)), nullptr);
  }

  void userSpacePatternRepeatsItsTile() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps pd = definition(Kind::Pattern, QStringLiteral("p"));
    pd.patternUserSpace = true;
    pd.x = px(0); pd.y = px(0); pd.width = px(20); pd.height = px(20);
    auto* pattern = add(defs, pd);
    add(pattern, rect(0, 0, 10, 10, Qt::red));
    add(pattern, rect(10, 10, 10, 10, Qt::blue));
    ElementProps r = rect(0, 0, 100, 100, Qt::black);
    r.fill = ref(QStringLiteral("p"));
    add(&s.root, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 5, 5, Qt::red);
    VERIFY_PX(img, 15, 15, Qt::blue);
    VERIFY_TRANSPARENT(img, 15, 5);
    VERIFY_PX(img, 25, 5, Qt::red);
    VERIFY_PX(img, 35, 35, Qt::blue);
    VERIFY_PX(img, 85, 85, Qt::red);
  }

  void boundingBoxPatternScalesWithTheElement() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps pd = definition(Kind::Pattern, QStringLiteral("p"));
    pd.patternUserSpace = false;
    pd.x = px(0); pd.y = px(0); pd.width = px(0.5); pd.height = px(0.5);
    auto* pattern = add(defs, pd);
    add(pattern, rect(0, 0, 25, 25, Qt::red));  // content units: user space
    ElementProps r = rect(20, 20, 60, 60, Qt::black);
    r.fill = ref(QStringLiteral("p"));
    add(&s.root, r);
    const QImage img = render(s.root);
    // The tile is 30x30 from (20,20); the red square fills 25 of it.
    VERIFY_PX(img, 30, 30, Qt::red);
    VERIFY_TRANSPARENT(img, 47, 30);   // the tile's empty strip
    VERIFY_PX(img, 60, 60, Qt::red);   // second tile row and column
  }

  void patternViewBoxMapsContentIntoTheTile() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps pd = definition(Kind::Pattern, QStringLiteral("p"));
    pd.patternUserSpace = true;
    pd.x = px(0); pd.y = px(0); pd.width = px(40); pd.height = px(40);
    pd.viewBox = ViewBox{true, 0, 0, 10, 10, QStringLiteral("xMidYMid"), 0};
    auto* pattern = add(defs, pd);
    add(pattern, rect(0, 0, 5, 5, Qt::red));
    ElementProps r = rect(0, 0, 80, 80, Qt::black);
    r.fill = ref(QStringLiteral("p"));
    add(&s.root, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 10, 10, Qt::red);       // 5 view box units are 20 px
    VERIFY_TRANSPARENT(img, 30, 30);
    VERIFY_PX(img, 50, 10, Qt::red);       // second tile
  }

  void patternFillOpacityApplies() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps pd = definition(Kind::Pattern, QStringLiteral("p"));
    pd.patternUserSpace = true;
    pd.x = px(0); pd.y = px(0); pd.width = px(20); pd.height = px(20);
    auto* pattern = add(defs, pd);
    add(pattern, rect(0, 0, 20, 20, Qt::red));
    ElementProps r = rect(0, 0, 100, 100, Qt::black);
    r.fill = ref(QStringLiteral("p"));
    r.fillOpacity = 0.5;
    r.propList = {QStringLiteral("fill"), QStringLiteral("fillOpacity")};
    add(&s.root, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 50, 50, QColor(255, 0, 0, 128));
  }

  void useOfASymbolFitsItsViewBox() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps sd = definition(Kind::Symbol, QStringLiteral("s"));
    sd.viewBox = ViewBox{true, 0, 0, 10, 10, QStringLiteral("xMidYMid"), 0};
    auto* symbol = add(defs, sd);
    add(symbol, rect(0, 0, 10, 10, Qt::green));
    add(symbol, rect(8, 8, 20, 20, Qt::blue));  // spills outside the viewport
    ElementProps u = make(Kind::Use);
    u.href = QStringLiteral("s");
    u.x = px(20); u.y = px(20); u.width = px(50); u.height = px(50);
    add(&s.root, u);
    const QImage img = render(s.root);
    VERIFY_PX(img, 30, 30, Qt::green);
    VERIFY_PX(img, 68, 68, Qt::blue);
    VERIFY_TRANSPARENT(img, 80, 80);   // the overflow is clipped
    VERIFY_TRANSPARENT(img, 10, 10);
  }

  void symbolIsNotDrawnByItself() {
    Scene s;
    ElementProps sd = definition(Kind::Symbol, QStringLiteral("s"));
    auto* symbol = add(&s.root, sd);
    add(symbol, rect(0, 0, 10, 10, Qt::green));
    const QImage img = render(s.root);
    VERIFY_TRANSPARENT(img, 5, 5);
  }

  ElementProps markerDef(const QString& id, double w, double h, double refX, double refY, const QString& orient,
                         bool strokeUnits) {
    ElementProps m = definition(Kind::Marker, id);
    m.markerWidth = px(w); m.markerHeight = px(h);
    m.refX = px(refX); m.refY = px(refY);
    m.orient = orient;
    m.markerStrokeWidthUnits = strokeUnits;
    return m;
  }

  void markerEndOrientsAlongThePath() {
    for (const bool rotate : {false, true}) {
      Scene s;
      auto* defs = add(&s.root, make(Kind::Defs));
      auto* marker = add(defs, markerDef(QStringLiteral("m"), 10, 4, 5, 2, rotate ? QStringLiteral("auto") : QString(), false));
      add(marker, rect(0, 0, 10, 4, Qt::red));
      ElementProps path = stroked(Kind::Path, Qt::black, 2);
      std::vector<PathVertex> v;
      path.path = parseSvgPath(QStringLiteral("M50 10 L50 70"), nullptr, &v);
      path.vertices = v;
      path.markerEnd = QStringLiteral("m");
      add(&s.root, path);
      const QImage img = render(s.root);
      VERIFY_PX(img, 50, 40, Qt::black);
      if (rotate) {
        VERIFY_PX(img, 50, 74, Qt::red);     // 10 long along the path direction: y 65..75
        VERIFY_TRANSPARENT(img, 54, 70);     // 4 wide across it: x 48..52
      } else {
        VERIFY_PX(img, 53, 70, Qt::red);     // 10 wide, 4 tall: x 45..55, y 68..72
        VERIFY_TRANSPARENT(img, 50, 75);
      }
    }
  }

  void markerUnitsScaleWithStrokeWidth() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    auto* marker = add(defs, markerDef(QStringLiteral("m"), 10, 4, 5, 2, QStringLiteral("auto"), true));
    add(marker, rect(0, 0, 10, 4, Qt::red));
    ElementProps path = stroked(Kind::Line, Qt::black, 3);
    path.x1 = px(10); path.y1 = px(50); path.x2 = px(60); path.y2 = px(50);
    path.markerEnd = QStringLiteral("m");
    add(&s.root, path);
    const QImage img = render(s.root);
    // Scaled by the stroke width 3: 30 long, 12 tall, centered on (60, 50).
    VERIFY_PX(img, 70, 50, Qt::red);
    VERIFY_PX(img, 60, 54, Qt::red);
    VERIFY_TRANSPARENT(img, 60, 58);
    VERIFY_TRANSPARENT(img, 80, 50);
  }

  void markerStartAndMidAreDrawnAtVertices() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    auto* mS = add(defs, markerDef(QStringLiteral("s"), 6, 6, 3, 3, QString(), false));
    add(mS, rect(0, 0, 6, 6, Qt::red));
    auto* mM = add(defs, markerDef(QStringLiteral("m"), 6, 6, 3, 3, QString(), false));
    add(mM, rect(0, 0, 6, 6, Qt::green));
    ElementProps path = stroked(Kind::Path, Qt::black, 1);
    std::vector<PathVertex> v;
    path.path = parseSvgPath(QStringLiteral("M10 50 L50 50 L90 50"), nullptr, &v);
    path.vertices = v;
    path.markerStart = QStringLiteral("s");
    path.markerMid = QStringLiteral("m");
    add(&s.root, path);
    const QImage img = render(s.root);
    VERIFY_PX(img, 10, 50, Qt::red);
    VERIFY_PX(img, 50, 50, Qt::green);
    VERIFY_TRANSPARENT(img, 90, 55);  // no end marker named
  }

  void markerContentUsesContextPaint() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    auto* marker = add(defs, markerDef(QStringLiteral("m"), 8, 8, 4, 4, QString(), false));
    ElementProps r = make(Kind::Rect);
    r.x = px(0); r.y = px(0); r.width = px(8); r.height = px(8);
    r.fill = Brush{Brush::Type::ContextStroke, {}, {}};
    r.propList = {QStringLiteral("fill")};
    add(marker, r);
    ElementProps path = stroked(Kind::Line, Qt::blue, 2);
    path.x1 = px(10); path.y1 = px(50); path.x2 = px(70); path.y2 = px(50);
    path.markerEnd = QStringLiteral("m");
    add(&s.root, path);
    const QImage img = render(s.root);
    VERIFY_PX(img, 70, 50, Qt::blue);
  }

  void gradientSpreadMethods() {
    auto probe = [](int spread) {
      Scene s;
      ElementProps g = definition(Kind::LinearGradient, QStringLiteral("g"));
      g.x1 = px(0.25); g.y1 = px(0); g.x2 = px(0.75); g.y2 = px(0);
      g.stops = {{0, Qt::red}, {1, Qt::blue}};
      g.spreadMethod = spread;
      add(&s.root, g);
      ElementProps r = rect(0, 0, 100, 100, Qt::black);
      r.fill = ref(QStringLiteral("g"));
      add(&s.root, r);
      return render(s.root).pixelColor(10, 50);
    };
    const QColor pad = probe(0), reflect = probe(1), repeat = probe(2);
    QVERIFY(pad.red() > 240 && pad.blue() < 15);           // before the first stop: its color
    QVERIFY(reflect.red() > reflect.blue());                // mirrored: near the red end
    QVERIFY(repeat.blue() > repeat.red() + 40);             // wrapped: near the blue end
  }

  void dashesAlternateAlongTheStroke() {
    Scene s;
    ElementProps l = line(0, 50, 100, 50, Qt::black, 4);
    l.strokeDasharray = {px(10), px(10)};
    l.propList.insert(QStringLiteral("strokeDasharray"));
    add(&s.root, l);
    const QImage img = render(s.root);
    VERIFY_PX(img, 5, 50, Qt::black);
    VERIFY_TRANSPARENT(img, 15, 50);
    VERIFY_PX(img, 25, 50, Qt::black);
    VERIFY_TRANSPARENT(img, 35, 50);
  }

  void dashOffsetShiftsThePattern() {
    Scene s;
    ElementProps l = line(0, 50, 100, 50, Qt::black, 4);
    l.strokeDasharray = {px(10), px(10)};
    l.strokeDashoffset = 10;
    l.propList.insert(QStringLiteral("strokeDasharray"));
    l.propList.insert(QStringLiteral("strokeDashoffset"));
    add(&s.root, l);
    const QImage img = render(s.root);
    VERIFY_TRANSPARENT(img, 5, 50);
    VERIFY_PX(img, 15, 50, Qt::black);
  }

  void lineCapsExtendTheStroke() {
    auto at = [](int cap, int x) {
      Scene s;
      ElementProps l = line(30, 50, 70, 50, Qt::black, 20);
      l.strokeLinecap = cap;
      l.propList.insert(QStringLiteral("strokeLinecap"));
      add(&s.root, l);
      return render(s.root).pixelColor(x, 50).alpha() > 128;
    };
    QVERIFY(!at(0, 25));   // butt
    QVERIFY(at(1, 25));    // round
    QVERIFY(at(2, 25));    // square
    QVERIFY(!at(1, 15));   // round stops at the radius
  }

  void miterLimitBevelsSharpCorners() {
    auto tip = [](double limit) {
      Scene s;
      ElementProps p = stroked(Kind::Path, Qt::black, 10);
      p.path = parseSvgPath(QStringLiteral("M10 80 L50 20 L90 80"));
      p.strokeLinejoin = 0;
      p.strokeMiterlimit = limit;
      p.propList.insert(QStringLiteral("strokeLinejoin"));
      p.propList.insert(QStringLiteral("strokeMiterlimit"));
      add(&s.root, p);
      return render(s.root).pixelColor(50, 14).alpha() > 128;  // inside the miter wedge, above where a bevel stops
    };
    QVERIFY(tip(10));
    QVERIFY(!tip(1.2));
  }

  void nonScalingStrokeKeepsItsWidthUnderAViewBox() {
    for (const bool nonScaling : {false, true}) {
      Scene s;
      RootProps rp;
      rp.vbWidth = 10; rp.vbHeight = 10;  // scales user units by 10
      s.root.setRootProps(rp);
      ElementProps l = line(0, 5, 10, 5, Qt::black, 2);
      l.nonScalingStroke = nonScaling;
      add(&s.root, l);
      const QImage img = render(s.root);
      VERIFY_PX(img, 50, 50, Qt::black);
      if (nonScaling) {
        VERIFY_TRANSPARENT(img, 50, 46);  // two item pixels wide: y 49..51
      } else {
        VERIFY_PX(img, 50, 46, Qt::black);  // twenty item pixels wide
      }
    }
  }

  void selfReferencingPaintServersAndMarkersTerminate() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    // A pattern whose content is filled by the pattern itself.
    ElementProps pd = definition(Kind::Pattern, QStringLiteral("p"));
    pd.patternUserSpace = true;
    pd.x = px(0); pd.y = px(0); pd.width = px(10); pd.height = px(10);
    auto* pattern = add(defs, pd);
    ElementProps inner = rect(0, 0, 10, 10, Qt::red);
    inner.fill = ref(QStringLiteral("p"));
    add(pattern, inner);
    // A marker whose content is a path that names the marker.
    auto* marker = add(defs, markerDef(QStringLiteral("m"), 10, 10, 5, 5, QString(), false));
    ElementProps mp = stroked(Kind::Line, Qt::black, 1);
    mp.x1 = px(0); mp.y1 = px(0); mp.x2 = px(10); mp.y2 = px(10);
    mp.markerStart = mp.markerEnd = QStringLiteral("m");
    add(marker, mp);
    // A clip path clipped by itself, and a mask masked by itself.
    ElementProps cd = definition(Kind::ClipPath, QStringLiteral("c"));
    cd.clipPath = QStringLiteral("c");
    add(add(defs, cd), rect(0, 0, 50, 50, Qt::black));
    ElementProps md = definition(Kind::Mask, QStringLiteral("k"));
    md.mask = QStringLiteral("k");
    add(add(defs, md), rect(0, 0, 50, 50, Qt::white));
    ElementProps r = rect(0, 0, 100, 100, Qt::black);
    r.fill = ref(QStringLiteral("p"));
    r.clipPath = QStringLiteral("c");
    r.mask = QStringLiteral("k");
    add(&s.root, r);
    ElementProps l = line(10, 10, 90, 90, Qt::black, 1);
    l.markerStart = l.markerMid = l.markerEnd = QStringLiteral("m");
    add(&s.root, l);
    render(s.root);  // terminating is the assertion
    QVERIFY(true);
  }

  void degenerateValuesDoNotCrash() {
    Scene s;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps pd = definition(Kind::Pattern, QStringLiteral("p"));
    pd.patternUserSpace = true;
    pd.x = px(0); pd.y = px(0); pd.width = px(1e12); pd.height = px(1e-9);
    auto* pattern = add(defs, pd);
    add(pattern, rect(0, 0, 1, 1, Qt::red));
    ElementProps pd2 = definition(Kind::Pattern, QStringLiteral("q"));
    pd2.patternUserSpace = true;
    pd2.width = px(nan); pd2.height = px(5);
    add(defs, pd2);
    for (const char* id : {"p", "q"}) {
      ElementProps r = rect(0, 0, 100, 100, Qt::black);
      r.fill = ref(QString::fromLatin1(id));
      add(&s.root, r);
    }
    ElementProps bad = rect(nan, inf, -inf, nan, Qt::red);
    add(&s.root, bad);
    ElementProps dashed = line(0, 0, 100, 100, Qt::black, 1);
    dashed.strokeDasharray = {px(1e-12), px(1e-12)};
    dashed.propList.insert(QStringLiteral("strokeDasharray"));
    add(&s.root, dashed);
    ElementProps g = make(Kind::Group);
    g.opacity = 0.5;
    g.matrix = QTransform(1e30, 0, 0, 1e30, 0, 0);
    add(add(&s.root, g), rect(0, 0, 1e30, 1e30, Qt::red));
    ElementProps m = make(Kind::Group);
    m.matrix = QTransform(0, 0, 0, 0, 0, 0);
    m.opacity = 0.5;
    add(add(&s.root, m), rect(0, 0, 10, 10, Qt::red));
    render(s.root);
    QVERIFY(true);
  }

  void nestedSvgPaintsInlineWithItsOwnViewBox() {
    Scene s(200, 100);
    auto* group = add(&s.root, make(Kind::Group));
    auto* nested = new RnsvgRootItem(group);
    nested->setSize(QSizeF(60, 60));
    RootProps rp;
    rp.bbWidth = px(60); rp.bbHeight = px(60);
    rp.vbWidth = 10; rp.vbHeight = 10;
    nested->setRootProps(rp);
    add(nested, rect(0, 0, 5, 5, Qt::red));      // 30x30 px
    add(nested, rect(5, 5, 20, 20, Qt::blue));   // spills past the nested viewport: clipped at 60
    add(&s.root, rect(100, 0, 20, 20, Qt::green));
    const QImage img = render(s.root);
    VERIFY_PX(img, 15, 15, Qt::red);
    VERIFY_PX(img, 45, 45, Qt::blue);
    VERIFY_TRANSPARENT(img, 70, 70);   // clipped to the nested 60x60
    VERIFY_PX(img, 110, 10, Qt::green);
    QVERIFY(nested->isNested());
    QVERIFY(!nested->isVisible());     // it paints through the outer root only
  }

  void nestedPropChangesScheduleTheVisiblePaintingRoot() {
    QQuickWindow window;
    window.resize(100, 100);
    Scene s(100, 100);
    s.root.setParentItem(window.contentItem());
    auto* nested = new RnsvgRootItem(&s.root);
    nested->setSize(QSizeF(60, 60));
    RootProps rp;
    rp.bbWidth = px(60); rp.bbHeight = px(60);
    nested->setRootProps(rp);
    auto* child = add(nested, rect(0, 0, 40, 40, Qt::red));
    QSignalSpy layouts(&s.root, &RnsvgRootItem::sceneLaidOut);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(layouts.count() > 0, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!window.grabWindow().isNull(), 2000);
    layouts.clear();
    child->setProps(rect(0, 0, 40, 40, Qt::blue));
    // No direct render()/layoutScene(): a retained descendant prop update must
    // schedule the outer window's actual painting root, not the hidden root.
    QTRY_VERIFY_WITH_TIMEOUT(layouts.count() > 0, 2000);
    const QImage frame = window.grabWindow();
    QVERIFY(!frame.isNull());
    QCOMPARE(frame.pixelColor(20, 20), QColor(Qt::blue));
  }

  void nestedSvgElementsTakeTouches() {
    Scene s(200, 100);
    auto* nested = new RnsvgRootItem(&s.root);
    nested->setSize(QSizeF(100, 100));
    RootProps rp;
    rp.bbWidth = px(100); rp.bbHeight = px(100);
    nested->setRootProps(rp);
    ElementProps r = rect(10, 10, 40, 40, Qt::red);
    r.responsible = true;
    auto* item = add(nested, r);
    QCOMPARE(s.root.hitElement(QPointF(30, 30)), item);
    QCOMPARE(s.root.hitElement(QPointF(90, 90)), nullptr);
  }

  void removingNestedSvgChildRepaintsVisibleRoot() {
    QQuickWindow window;
    window.setColor(Qt::white);
    window.resize(100, 100);
    Scene s(100, 100);
    s.root.setParentItem(window.contentItem());
    auto* nested = new RnsvgRootItem(&s.root);
    nested->setSize(QSizeF(60, 60));
    RootProps rp;
    rp.bbWidth = px(60); rp.bbHeight = px(60);
    nested->setRootProps(rp);
    auto* child = add(nested, rect(0, 0, 40, 40, Qt::red));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(s.root.paintCount() > 0, 2000);
    QCOMPARE(window.grabWindow().pixelColor(20, 20), QColor(Qt::red));
    const int before = s.root.paintCount();
    // Fabric removes the item before deleting it. Its old root must invalidate
    // the texture even though the child's parentChanged now sees no parent.
    child->setParentItem(nullptr);
    delete child;
    QTRY_VERIFY_WITH_TIMEOUT(s.root.paintCount() > before, 2000);
    QCOMPARE(window.grabWindow().pixelColor(20, 20), QColor(Qt::white));
  }

  void nestedRootsWithinNestedRootsContributeToGroupBounds() {
    Scene s(200, 200);
    ElementProps g = make(Kind::Group);
    g.opacity = 0.5;
    auto* group = add(&s.root, g);
    auto* nested = new RnsvgRootItem(group);
    nested->setSize(QSizeF(100, 100));
    RootProps rp;
    rp.bbWidth = px(100); rp.bbHeight = px(100);
    rp.vbWidth = 50; rp.vbHeight = 50;
    nested->setRootProps(rp);
    auto* inner = new RnsvgRootItem(nested);
    inner->setSize(QSizeF(40, 40));
    RootProps ip;
    ip.bbWidth = px(40); ip.bbHeight = px(40);
    ip.vbWidth = 20; ip.vbHeight = 20;
    inner->setRootProps(ip);
    add(inner, rect(5, 5, 30, 30, Qt::red));
    // Both viewBox scales apply, and the inner viewport clips at outer x/y=80.
    QCOMPARE(s.root.elementBounds(group), QRectF(20, 20, 60, 60));
    const QImage img = render(s.root);
    VERIFY_PX(img, 30, 30, QColor(255, 0, 0, 128));
    VERIFY_TRANSPARENT(img, 85, 85);
  }

  void foreignObjectIsPlacedThroughEveryAncestorTransform() {
    Scene s(200, 200);
    RootProps rp;
    rp.vbWidth = 100; rp.vbHeight = 100;   // scale 2
    s.root.setRootProps(rp);
    ElementProps g = make(Kind::Group);
    g.matrix = QTransform::fromTranslate(10, 20);
    auto* group = add(&s.root, g);
    auto* fo = new RnsvgForeignObjectItem(group);
    ElementProps p = make(Kind::ForeignObject);
    p.x = px(5); p.y = px(5); p.width = px(30); p.height = px(40);
    fo->setProps(p);
    s.root.layoutScene();
    QVERIFY(fo->isPlaced());
    QCOMPARE(fo->boxSize(), QSizeF(30, 40));
    // The box corner: ((5 + 10) * 2, (5 + 20) * 2) in item space.
    QCOMPARE(fo->mapToItem(&s.root, QPointF(0, 0)), QPointF(30, 50));
    // One unit of the box is two item pixels.
    QCOMPARE(fo->mapToItem(&s.root, QPointF(10, 0)), QPointF(50, 50));
    // display:none removes the placement.
    p.displayNone = true;
    fo->setProps(p);
    s.root.layoutScene();
    QVERIFY(!fo->isPlaced());
  }

  void elementBoundsAreInRootCoordinates() {
    Scene s(200, 200);
    RootProps rp;
    rp.vbWidth = 100; rp.vbHeight = 100;
    s.root.setRootProps(rp);
    ElementProps g = make(Kind::Group);
    g.matrix = QTransform::fromTranslate(10, 0);
    auto* group = add(&s.root, g);
    auto* r = add(group, rect(5, 5, 20, 10, Qt::red));
    const QRectF b = s.root.elementBounds(r);
    QCOMPARE(b, QRectF(30, 10, 40, 20));
    QVERIFY(s.root.elementBounds(group).contains(b));
  }

  void boundsOfManyElementsTakeOneTraversal() {
    Scene s(400, 400);
    RootProps rp;
    rp.vbWidth = 100; rp.vbHeight = 100;
    s.root.setRootProps(rp);
    constexpr int kGroups = 40, kPerGroup = 49;  // 40 groups + 1960 rects = 2000
    QSet<const RnsvgElementItem*> all;
    QList<RnsvgElementItem*> sample;
    for (int g = 0; g < kGroups; ++g) {
      ElementProps gp = make(Kind::Group);
      gp.matrix = QTransform::fromTranslate(g, 0);
      auto* group = add(&s.root, gp);
      all.insert(group);
      for (int i = 0; i < kPerGroup; ++i) {
        auto* r = add(group, rect(i % 7, i / 7, 3, 2, Qt::red));
        all.insert(r);
        if ((g * kPerGroup + i) % 97 == 0) sample.append(r);
      }
      sample.append(group);
    }
    const int n = int(all.size());
    QCOMPARE(n, 2000);
    int visits = 0;
    const auto bounds = boundsOfElements(s.root, all, &visits);
    QVERIFY2(visits <= 3 * n, qPrintable(QStringLiteral("visits=%1 for %2 elements").arg(visits).arg(n)));
    QCOMPARE(int(bounds.size()), n);
    for (const auto* e : sample) QCOMPARE(bounds.value(e), s.root.elementBounds(e));
    // The root's own pass fills the same cache its reporters read.
    for (const auto* e : all) s.root.watchBounds(e);
    s.root.layoutScene();
    for (const auto* e : sample) QCOMPARE(s.root.reportedBounds(e), s.root.elementBounds(e));
  }

  void clipPathCyclesWithBranchingTerminateQuickly() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps cd = definition(Kind::ClipPath, QStringLiteral("c"));
    auto* clip = add(defs, cd);
    for (int i = 0; i < 4; ++i) {
      ElementProps r = rect(0, 0, 50, 50, Qt::black);
      r.clipPath = QStringLiteral("c");   // each child is clipped by the clip path it is in
      add(clip, r);
    }
    ElementProps g = definition(Kind::Group, QStringLiteral("g"));
    auto* group = add(defs, g);
    for (int i = 0; i < 3; ++i) {
      ElementProps u = make(Kind::Use);
      u.href = QStringLiteral("g");     // a group that uses itself three times
      add(group, u);
    }
    ElementProps r = rect(0, 0, 100, 100, Qt::red);
    r.clipPath = QStringLiteral("c");
    r.opacity = 0.5;
    add(&s.root, r);
    ElementProps u = make(Kind::Use);
    u.href = QStringLiteral("g");
    u.opacity = 0.5;
    add(&s.root, u);
    QElapsedTimer timer;
    timer.start();
    render(s.root);
    QVERIFY2(timer.elapsed() < 5000, qPrintable(QString::number(timer.elapsed())));
  }

  void markerThatUsesItselfTerminatesWhenMeasured() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    auto* marker = add(defs, markerDef(QStringLiteral("m"), 10, 10, 5, 5, QString(), false));
    ElementProps inside = stroked(Kind::Path, Qt::black, 1);
    std::vector<PathVertex> v;
    inside.path = parseSvgPath(QStringLiteral("M0 0 L5 0 L5 5 L0 5 L0 0"), nullptr, &v);
    inside.vertices = v;
    inside.markerStart = inside.markerMid = inside.markerEnd = QStringLiteral("m");
    add(marker, inside);
    ElementProps g = make(Kind::Group);
    g.opacity = 0.5;   // the group needs its painted box, which measures the markers
    auto* group = add(&s.root, g);
    ElementProps outer = inside;
    outer.path = parseSvgPath(QStringLiteral("M20 20 L60 20 L60 60 L20 60 L20 20"), nullptr, &v);
    outer.vertices = v;
    add(group, outer);
    QElapsedTimer timer;
    timer.start();
    render(s.root);
    QVERIFY2(timer.elapsed() < 5000, qPrintable(QString::number(timer.elapsed())));
  }

  void useOfASymbolWithAViewBoxKeepsItsWholeLayer() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps sd = definition(Kind::Symbol, QStringLiteral("s"));
    sd.viewBox = ViewBox{true, 0, 0, 10, 10, QStringLiteral("xMidYMid"), 0};
    add(add(defs, sd), rect(0, 0, 10, 10, Qt::red));
    ElementProps u = make(Kind::Use);
    u.href = QStringLiteral("s");
    u.width = px(100); u.height = px(100);
    u.opacity = 0.5;
    add(&s.root, u);
    const QImage img = render(s.root);
    VERIFY_PX(img, 50, 50, QColor(255, 0, 0, 128));
    VERIFY_PX(img, 95, 95, QColor(255, 0, 0, 128));
  }

  void groupWithNestedSvgKeepsItsContentWhenItNeedsALayer() {
    Scene s(200, 100);
    ElementProps g = make(Kind::Group);
    g.opacity = 0.5;
    auto* group = add(&s.root, g);
    auto* nested = new RnsvgRootItem(group);
    nested->setSize(QSizeF(60, 60));
    RootProps rp;
    rp.bbWidth = px(60); rp.bbHeight = px(60);
    nested->setRootProps(rp);
    add(nested, rect(0, 0, 60, 60, Qt::red));
    const QImage img = render(s.root);
    VERIFY_PX(img, 20, 20, QColor(255, 0, 0, 128));
  }

  void nestedSvgDirectlyUnderTheRootPaintsOnce() {
    Scene s(200, 100);
    auto* nested = new RnsvgRootItem(&s.root);
    nested->setSize(QSizeF(60, 60));
    RootProps rp;
    rp.bbWidth = px(60); rp.bbHeight = px(60);
    nested->setRootProps(rp);
    add(nested, rect(0, 0, 60, 60, Qt::red));
    QVERIFY(nested->isNested());
    QVERIFY(!nested->isVisible());
    const QImage img = render(s.root);
    VERIFY_PX(img, 20, 20, Qt::red);
  }

  void foreignObjectInsideDefsIsNotPlaced() {
    Scene s(100, 100);
    auto* defs = add(&s.root, make(Kind::Defs));
    auto* fo = new RnsvgForeignObjectItem(defs);
    ElementProps p = make(Kind::ForeignObject);
    p.width = px(10); p.height = px(10);
    fo->setProps(p);
    s.root.layoutScene();
    QVERIFY(!fo->isPlaced());
  }

  void symbolViewportClipsTouches() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps sd = definition(Kind::Symbol, QStringLiteral("s"));
    auto* symbol = add(defs, sd);
    ElementProps r = rect(0, 0, 80, 80, Qt::red);
    r.responsible = true;
    auto* inner = add(symbol, r);
    ElementProps u = make(Kind::Use);
    u.href = QStringLiteral("s");
    u.width = px(40); u.height = px(40);
    add(&s.root, u);
    QCOMPARE(s.root.hitElement(QPointF(20, 20)), inner);
    QCOMPARE(s.root.hitElement(QPointF(60, 60)), nullptr);  // inside the rect, outside the viewport
  }

  void invalidViewBoxIsIgnored() {
    Scene s(100, 100);
    RootProps rp;
    rp.bbWidth = px(100); rp.bbHeight = px(100);
    rp.vbWidth = std::nan(""); rp.vbHeight = 10;
    s.root.setRootProps(rp);
    add(&s.root, rect(0, 0, 50, 50, Qt::red));
    const QImage img = render(s.root);
    VERIFY_PX(img, 25, 25, Qt::red);   // drawn at scale 1, as with no viewBox
    VERIFY_PX(img, 75, 75, QColor(0, 0, 0, 0));
    rp.vbWidth = -10; rp.vbHeight = std::numeric_limits<double>::infinity();
    QVERIFY(!rp.hasViewBox());
  }

  void pointerEventsNoneShieldsLiveChildren() {
    Scene s;
    ElementProps gp = make(Kind::Group);
    auto* group = add(&s.root, gp);
    QVERIFY(group->passesTouchesToChildren());
    gp.pointerEvents = QStringLiteral("none");
    group->setProps(gp);
    QVERIFY(!group->passesTouchesToChildren());
    gp.pointerEvents = QStringLiteral("box-only");
    group->setProps(gp);
    QVERIFY(!group->passesTouchesToChildren());
    gp.pointerEvents = QStringLiteral("box-none");
    group->setProps(gp);
    QVERIFY(group->passesTouchesToChildren());
    gp.pointerEvents.clear();
    gp.displayNone = true;
    group->setProps(gp);
    QVERIFY(!group->passesTouchesToChildren());
  }

  void useGroupOpacityIsApplied() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps gd = definition(Kind::Group, QStringLiteral("g"));
    auto* g = add(defs, gd);
    add(g, rect(0, 0, 60, 60, Qt::red));
    add(g, rect(30, 30, 60, 60, Qt::red));
    ElementProps u = make(Kind::Use);
    u.href = QStringLiteral("g");
    u.opacity = 0.5;
    add(&s.root, u);
    const QImage img = render(s.root);
    VERIFY_PX(img, 45, 45, QColor(255, 0, 0, 128));
  }
};

QTEST_MAIN(RnsvgSceneTest)
#include "RnsvgSceneTest.moc"
