// Scene-level tests: clip rules, layer sizing, hit testing through clip, mask
// and pointerEvents, patterns, symbols, markers, gradient spread, dashes and
// non-scaling strokes. Same method as RnsvgRenderTest.cpp: build the retained
// tree directly, paint, probe pixels well inside or outside edges.
#include "RnsvgTestKit.h"

#include <QPointer>

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
    // The first vertex of a closed subpath also receives the closing segment.
    QVERIFY(qAbs(v[0].inDirection.x() + 0.7071) < 0.001 && qAbs(v[0].inDirection.y() + 0.7071) < 0.001);
    // An arc is one segment: two vertices, not one per cubic.
    parseSvgPath(QStringLiteral("M0 0 A10 10 0 1 1 20 0"), nullptr, &v);
    QCOMPARE(int(v.size()), 2);
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
      if (rule == 0) {
        VERIFY_TRANSPARENT(img, 50, 50);  // the middle pentagon is a hole
      } else {
        VERIFY_PX(img, 50, 50, Qt::red);  // unset and nonzero fill it
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
