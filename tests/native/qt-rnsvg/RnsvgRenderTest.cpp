// Golden-pixel tests for the react-native-svg element renderer. Scenes are
// built directly from retained element props (what the Fabric bridge produces),
// painted with the real renderer and probed at known pixels. Probes sit well
// inside shapes or well outside edges so anti-aliasing differences between Qt
// builds cannot change a verdict.
#include "RnsvgItems.h"
#include "RnsvgPath.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPointer>
#include <QTest>
#include <QFontDatabase>

using namespace rnqnx::views;
using namespace rnqnx::views::rnsvg;

namespace {

Length px(double v) { return Length{v, Length::Unit::Px, true}; }
Length pct(double v) { return Length{v, Length::Unit::Percent, true}; }

Brush color(const QColor& c) { return Brush{Brush::Type::Color, c, {}}; }
Brush ref(const QString& id) { return Brush{Brush::Type::Ref, {}, id}; }

ElementProps make(Kind kind) {
  ElementProps p;
  p.kind = kind;
  p.fill = color(Qt::black);
  return p;
}

// A shape that sets its own fill, the way extractFill reports it.
ElementProps filled(Kind kind, const QColor& c) {
  ElementProps p = make(kind);
  p.fill = color(c);
  p.propList = {QStringLiteral("fill")};
  return p;
}

RnsvgElementItem* add(QQuickItem* parent, ElementProps p) {
  const Kind k = p.kind;
  auto* item = new RnsvgElementItem(k, parent);
  item->setProps(std::move(p));
  return item;
}

ElementProps rect(double x, double y, double w, double h, const QColor& c) {
  ElementProps p = filled(Kind::Rect, c);
  p.x = px(x); p.y = px(y); p.width = px(w); p.height = px(h);
  return p;
}

ElementProps circle(double cx, double cy, double r, const QColor& c) {
  ElementProps p = filled(Kind::Circle, c);
  p.cx = px(cx); p.cy = px(cy); p.r = px(r);
  return p;
}

QImage render(RnsvgRootItem& root) {
  const qreal dpr = 1.0;
  QImage image(QSize(qCeil(root.width() * dpr), qCeil(root.height() * dpr)), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  root.paint(&painter);
  painter.end();
  return image;
}

bool near(const QColor& a, const QColor& b, int tol = 4) {
  return std::abs(a.red() - b.red()) <= tol && std::abs(a.green() - b.green()) <= tol &&
         std::abs(a.blue() - b.blue()) <= tol && std::abs(a.alpha() - b.alpha()) <= tol;
}

#define VERIFY_PX(img, x, y, expected)                                                      \
  do {                                                                                      \
    const QColor got = (img).pixelColor(x, y);                                              \
    if (!near(got, expected)) {                                                             \
      QFAIL(qPrintable(QStringLiteral("pixel (%1,%2) is %3, expected %4")                   \
                           .arg(x).arg(y).arg(got.name(QColor::HexArgb), QColor(expected).name(QColor::HexArgb)))); \
    }                                                                                       \
  } while (0)

#define VERIFY_TRANSPARENT(img, x, y) \
  QVERIFY2((img).pixelColor(x, y).alpha() <= 4, qPrintable(QStringLiteral("(%1,%2) should be empty").arg(x).arg(y)))

struct Scene {
  RnsvgRootItem root;
  explicit Scene(double w = 100, double h = 100) {
    root.setSize(QSizeF(w, h));
    RootProps rp;
    root.setRootProps(rp);
  }
};

}  // namespace

class RnsvgRenderTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void pathParserHandlesAllCommands() {
    bool ok = false;
    QPainterPath p = parseSvgPath(QStringLiteral("M10 10 h20 v20 H10 z"), &ok);
    QVERIFY(ok);
    QCOMPARE(p.boundingRect(), QRectF(10, 10, 20, 20));
    p = parseSvgPath(QStringLiteral("m10,10 20,0 0,20 -20,0z"), &ok);
    QVERIFY(ok);
    QCOMPARE(p.boundingRect(), QRectF(10, 10, 20, 20));
    p = parseSvgPath(QStringLiteral("M0 0 C 10 0 10 20 20 20 S 30 40 40 40 Q 50 40 50 50 T 60 60"), &ok);
    QVERIFY(ok);
    QVERIFY(p.boundingRect().right() >= 60 - 0.001);
    // Half circle of radius 10 through the packed arc flags form.
    p = parseSvgPath(QStringLiteral("M0 10 A10 10 0 0110 0"), &ok);
    QVERIFY(ok);
    QVERIFY(qAbs(p.boundingRect().width() - 10) < 0.1);
    QVERIFY(qAbs(p.boundingRect().height() - 10) < 0.1);
    // Radii too small are scaled up, so the arc still reaches its end point.
    p = parseSvgPath(QStringLiteral("M0 0 A1 1 0 0 1 100 0"), &ok);
    QVERIFY(ok);
    QVERIFY(qAbs(p.currentPosition().x() - 100) < 0.001);
  }

  void pathParserKeepsValidPrefixOnError() {
    bool ok = true;
    const QPainterPath p = parseSvgPath(QStringLiteral("M0 0 L10 10 L20 oops"), &ok);
    QVERIFY(!ok);
    QCOMPARE(p.elementCount(), 2);
    parseSvgPath(QStringLiteral("L10 10"), &ok);
    QVERIFY(!ok);  // path data must start with a moveto
    parseSvgPath(QStringLiteral("M0 0 X"), &ok);
    QVERIFY(!ok);
  }

  void primitivesFillAndStroke() {
    Scene s;
    add(&s.root, rect(10, 10, 30, 30, Qt::red));
    add(&s.root, circle(70, 70, 20, Qt::blue));
    ElementProps e = filled(Kind::Ellipse, Qt::green);
    e.cx = px(30); e.cy = px(75); e.rx = px(20); e.ry = px(10);
    add(&s.root, e);
    ElementProps line = make(Kind::Line);
    line.x1 = px(50); line.y1 = px(10); line.x2 = px(90); line.y2 = px(10);
    line.stroke = color(Qt::black);
    line.strokeWidth = px(6);
    line.propList = {QStringLiteral("stroke"), QStringLiteral("strokeWidth"), QStringLiteral("fill")};
    line.fill = Brush{};
    add(&s.root, line);
    const QImage img = render(s.root);
    VERIFY_PX(img, 25, 25, Qt::red);
    VERIFY_PX(img, 70, 70, Qt::blue);
    VERIFY_PX(img, 30, 75, Qt::green);
    VERIFY_PX(img, 70, 10, Qt::black);
    VERIFY_TRANSPARENT(img, 70, 17);
    VERIFY_TRANSPARENT(img, 5, 5);
    VERIFY_TRANSPARENT(img, 45, 45);
    // A circle's corner lies outside its radius.
    VERIFY_TRANSPARENT(img, 54, 54);
  }

  void rectRoundedCornersAndPercentLengths() {
    Scene s;
    ElementProps r = rect(0, 0, 0, 0, Qt::red);
    r.x = pct(10); r.y = pct(10); r.width = pct(80); r.height = pct(80); r.rx = px(20);
    add(&s.root, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 50, 50, Qt::red);
    VERIFY_TRANSPARENT(img, 11, 11);  // inside the rounded corner
    VERIFY_PX(img, 30, 12, Qt::red);
    VERIFY_TRANSPARENT(img, 5, 50);
  }

  void viewBoxMapsUserSpace() {
    Scene s;
    RootProps rp;
    rp.vbWidth = 10; rp.vbHeight = 10;
    s.root.setRootProps(rp);
    add(&s.root, rect(0, 0, 5, 5, Qt::red));
    add(&s.root, rect(5, 5, 5, 5, Qt::blue));
    QImage img = render(s.root);
    VERIFY_PX(img, 25, 25, Qt::red);
    VERIFY_PX(img, 75, 75, Qt::blue);
    VERIFY_TRANSPARENT(img, 75, 25);
    // meet centers a non-square viewBox; slice fills and clips.
    s.root.setSize(QSizeF(200, 100));
    img = render(s.root);
    VERIFY_PX(img, 75, 25, Qt::red);
    VERIFY_TRANSPARENT(img, 25, 50);
    rp.meetOrSlice = 1;
    s.root.setRootProps(rp);
    img = render(s.root);
    VERIFY_PX(img, 25, 25, Qt::red);
    // none stretches independently on each axis.
    rp.meetOrSlice = 2;
    s.root.setRootProps(rp);
    img = render(s.root);
    VERIFY_PX(img, 90, 20, Qt::red);
    VERIFY_PX(img, 110, 80, Qt::blue);
  }

  void fillRuleEvenOddPunchesHole() {
    Scene s;
    ElementProps p = filled(Kind::Path, Qt::red);
    p.path = parseSvgPath(QStringLiteral("M10 10 H90 V90 H10Z M30 30 H70 V70 H30Z"));
    p.fillEvenOdd = true;
    p.propList = {QStringLiteral("fill"), QStringLiteral("fillRule")};
    add(&s.root, p);
    QImage img = render(s.root);
    VERIFY_PX(img, 20, 20, Qt::red);
    VERIFY_TRANSPARENT(img, 50, 50);
  }

  void inheritanceFollowsPropList() {
    Scene s;
    ElementProps g = make(Kind::Group);
    g.fill = color(Qt::green);
    g.fillOpacity = 1;
    g.propList = {QStringLiteral("fill")};
    auto* group = add(&s.root, g);
    // No propList: inherits the group's fill despite carrying a black default.
    add(group, [] { auto p = make(Kind::Rect); p.x = px(0); p.y = px(0); p.width = px(40); p.height = px(40); return p; }());
    // Own fill wins over the inherited one.
    add(group, rect(50, 0, 40, 40, Qt::red));
    // Stroke inherited from a second group, fill from the first.
    ElementProps outer = make(Kind::Group);
    outer.stroke = color(Qt::blue);
    outer.strokeWidth = px(10);
    outer.propList = {QStringLiteral("stroke"), QStringLiteral("strokeWidth")};
    auto* og = add(&s.root, outer);
    ElementProps r = make(Kind::Rect);
    r.x = px(20); r.y = px(55); r.width = px(60); r.height = px(30);
    add(og, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 20, 20, Qt::green);
    VERIFY_PX(img, 70, 20, Qt::red);
    VERIFY_PX(img, 50, 70, Qt::black);   // default fill, blue stroke
    VERIFY_PX(img, 50, 55, Qt::blue);    // stroke centered on the top edge
  }

  void currentColorUsesNearestColor() {
    Scene s;
    ElementProps g = make(Kind::Group);
    g.color = QColor(Qt::magenta);
    add(&s.root, g);
    auto* group = s.root.elementChildren().first();
    ElementProps r = make(Kind::Rect);
    r.x = px(10); r.y = px(10); r.width = px(50); r.height = px(50);
    r.fill = Brush{Brush::Type::CurrentColor, {}, {}};
    r.propList = {QStringLiteral("fill")};
    add(group, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 30, 30, Qt::magenta);
  }

  void opacityAppliesToGroupAsAUnit() {
    Scene s;
    ElementProps g = make(Kind::Group);
    g.opacity = 0.5;
    auto* group = add(&s.root, g);
    add(group, rect(0, 0, 60, 60, Qt::red));
    add(group, rect(30, 30, 60, 60, Qt::red));
    ElementProps single = rect(0, 90, 100, 10, Qt::blue);
    single.fillOpacity = 0.5;
    single.propList.insert(QStringLiteral("fillOpacity"));
    add(&s.root, single);
    const QImage img = render(s.root);
    // The overlap of two opaque children inside a 50% group is 50%, not 75%.
    VERIFY_PX(img, 45, 45, QColor(255, 0, 0, 128));
    VERIFY_PX(img, 15, 30, QColor(255, 0, 0, 128));
    VERIFY_PX(img, 50, 95, QColor(0, 0, 255, 128));
  }

  void linearGradientFillsObjectBoundingBox() {
    Scene s;
    ElementProps defs = make(Kind::Defs);
    auto* d = add(&s.root, defs);
    ElementProps g = make(Kind::LinearGradient);
    g.name = QStringLiteral("grad");
    g.x1 = px(0); g.y1 = px(0); g.x2 = px(1); g.y2 = px(0);
    g.stops = {{0, Qt::red}, {1, Qt::blue}};
    add(d, g);
    ElementProps r = make(Kind::Rect);
    r.x = px(0); r.y = px(0); r.width = px(100); r.height = px(100);
    r.fill = ref(QStringLiteral("grad"));
    r.propList = {QStringLiteral("fill")};
    add(&s.root, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 2, 50, QColor(250, 0, 5));
    VERIFY_PX(img, 97, 50, QColor(5, 0, 250));
    const QColor mid = img.pixelColor(50, 50);
    QVERIFY(qAbs(mid.red() - 127) < 8 && qAbs(mid.blue() - 127) < 8);
  }

  void radialGradientAndUserSpaceUnits() {
    Scene s;
    ElementProps g = make(Kind::RadialGradient);
    g.name = QStringLiteral("rad");
    g.cx = px(50); g.cy = px(50); g.rx = px(50); g.ry = px(50);
    g.gradientUserSpace = true;
    g.stops = {{0, Qt::white}, {1, Qt::black}};
    add(&s.root, g);
    ElementProps r = make(Kind::Rect);
    r.x = px(0); r.y = px(0); r.width = px(100); r.height = px(100);
    r.fill = ref(QStringLiteral("rad"));
    r.propList = {QStringLiteral("fill")};
    add(&s.root, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 50, 50, Qt::white);
    QVERIFY(img.pixelColor(50, 90).red() < 70);
    QVERIFY(img.pixelColor(50, 70).red() > img.pixelColor(50, 90).red());
  }

  void gradientStopAlphaAndMissingServer() {
    Scene s;
    ElementProps g = make(Kind::LinearGradient);
    g.name = QStringLiteral("fade");
    g.x1 = px(0); g.y1 = px(0); g.x2 = px(1); g.y2 = px(0);
    g.stops = {{0, QColor(255, 0, 0, 255)}, {1, QColor(255, 0, 0, 0)}};
    add(&s.root, g);
    ElementProps r = make(Kind::Rect);
    r.x = px(0); r.y = px(0); r.width = px(100); r.height = px(50);
    r.fill = ref(QStringLiteral("fade"));
    r.propList = {QStringLiteral("fill")};
    add(&s.root, r);
    ElementProps missing = make(Kind::Rect);
    missing.x = px(0); missing.y = px(60); missing.width = px(100); missing.height = px(40);
    missing.fill = ref(QStringLiteral("nope"));
    missing.propList = {QStringLiteral("fill")};
    add(&s.root, missing);
    const QImage img = render(s.root);
    QVERIFY(img.pixelColor(3, 25).alpha() > 240);
    QVERIFY(img.pixelColor(96, 25).alpha() < 20);
    VERIFY_TRANSPARENT(img, 50, 80);
  }

  void useInstantiatesDefinitionWithTranslation() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps r = rect(0, 0, 20, 20, Qt::red);
    r.name = QStringLiteral("box");
    add(defs, r);
    VERIFY_TRANSPARENT(render(s.root), 10, 10);  // a definition alone paints nothing
    ElementProps u = make(Kind::Use);
    u.href = QStringLiteral("box");
    u.x = px(40); u.y = px(40);
    add(&s.root, u);
    ElementProps u2 = u;
    u2.x = px(0); u2.y = px(70);
    u2.matrix = QTransform::fromTranslate(60, 0);
    add(&s.root, u2);
    const QImage img = render(s.root);
    VERIFY_PX(img, 50, 50, Qt::red);
    VERIFY_PX(img, 70, 80, Qt::red);
    VERIFY_TRANSPARENT(img, 10, 10);
  }

  void clipPathRestrictsPainting() {
    Scene s;
    auto* defs = add(&s.root, make(Kind::Defs));
    ElementProps cp = make(Kind::ClipPath);
    cp.name = QStringLiteral("clip");
    auto* clip = add(defs, cp);
    add(clip, circle(50, 50, 30, Qt::black));
    ElementProps r = rect(0, 0, 100, 100, Qt::red);
    r.clipPath = QStringLiteral("clip");
    add(&s.root, r);
    const QImage img = render(s.root);
    VERIFY_PX(img, 50, 50, Qt::red);
    VERIFY_TRANSPARENT(img, 5, 5);
    VERIFY_TRANSPARENT(img, 50, 90);
  }

  void maskModulatesByLuminanceAndAlpha() {
    Scene s;
    ElementProps m = make(Kind::Mask);
    m.name = QStringLiteral("fadeMask");
    m.maskUserSpace = true;
    m.x = px(0); m.y = px(0); m.width = px(100); m.height = px(100);
    auto* mask = add(&s.root, m);
    add(mask, rect(0, 0, 50, 100, Qt::white));   // left half passes
    add(mask, rect(50, 0, 50, 50, QColor(128, 128, 128)));  // top-right passes ~50%
    ElementProps r = rect(0, 0, 100, 100, Qt::red);
    r.mask = QStringLiteral("fadeMask");
    add(&s.root, r);
    QImage img = render(s.root);
    VERIFY_PX(img, 25, 25, Qt::red);
    VERIFY_PX(img, 75, 25, QColor(255, 0, 0, 128));
    VERIFY_TRANSPARENT(img, 75, 75);
    // Alpha masks ignore the luminance of the mask content.
    ElementProps m2 = mask->props();
    m2.maskAlpha = true;
    mask->setProps(m2);
    img = render(s.root);
    VERIFY_PX(img, 75, 25, Qt::red);
  }

  void textRendersAndAnchors() {
    if (QFontDatabase::families().isEmpty()) QSKIP("no fonts available to the offscreen platform");
    Scene s(200, 60);
    ElementProps t = make(Kind::Text);
    t.tx = {px(100)}; t.ty = {px(40)};
    t.fill = color(Qt::red);
    t.propList = {QStringLiteral("fill")};
    t.font.size = 32;
    t.font.anchor = QStringLiteral("middle");
    auto* text = add(&s.root, t);
    ElementProps span = make(Kind::TSpan);
    span.content = QStringLiteral("HHHH");
    add(text, span);
    QImage img = render(s.root);
    auto extent = [&](const QImage& image) {
      int lo = image.width(), hi = -1;
      for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
          if (image.pixelColor(x, y).alpha() > 128) { lo = std::min(lo, x); hi = std::max(hi, x); }
      return QPair<int, int>(lo, hi);
    };
    const auto mid = extent(img);
    QVERIFY2(mid.second > mid.first, "text painted nothing");
    QVERIFY(mid.first < 100 && mid.second > 100);
    QCOMPARE(img.pixelColor((mid.first + mid.second) / 2, 5).alpha(), 0);  // above the cap height
    ElementProps t2 = text->props();
    t2.font.anchor = QStringLiteral("start");
    text->setProps(t2);
    img = render(s.root);
    const auto start = extent(img);
    QVERIFY(start.first >= 98);
    QVERIFY(start.first > mid.first + 20);
  }

  void imageDrawsWithAspectRatio() {
    Scene s;
    QImage src(20, 10, QImage::Format_ARGB32);
    src.fill(Qt::green);
    ElementProps i = make(Kind::Image);
    i.image = src;
    i.x = px(0); i.y = px(0); i.width = px(100); i.height = px(100);
    add(&s.root, i);  // xMidYMid meet: 100x50 centered vertically
    const QImage img = render(s.root);
    VERIFY_PX(img, 50, 50, Qt::green);
    VERIFY_TRANSPARENT(img, 50, 10);
    VERIFY_TRANSPARENT(img, 50, 90);
  }

  void hitTestingPicksTopmostResponsibleElement() {
    Scene s;
    ElementProps back = rect(0, 0, 100, 100, Qt::red);
    back.responsible = true;
    auto* backItem = add(&s.root, back);
    ElementProps front = circle(50, 50, 20, Qt::blue);
    front.responsible = true;
    auto* frontItem = add(&s.root, front);
    ElementProps passive = rect(0, 0, 20, 20, Qt::green);  // painted, not responsible
    add(&s.root, passive);
    QCOMPARE(s.root.hitElement(QPointF(50, 50)), frontItem);
    QCOMPARE(s.root.hitElement(QPointF(90, 90)), backItem);
    QCOMPARE(s.root.hitElement(QPointF(10, 10)), backItem);  // passes through the passive shape
    QCOMPARE(s.root.hitElement(QPointF(150, 50)), nullptr);
  }

  void hitTestingHonorsTransformsGroupsAndPointerEvents() {
    Scene s;
    ElementProps g = make(Kind::Group);
    g.responsible = true;
    g.matrix = QTransform::fromTranslate(50, 0).scale(2, 2);
    auto* group = add(&s.root, g);
    add(group, rect(0, 0, 20, 20, Qt::red));  // covers 50..90 x 0..40 on screen
    auto* groupItem = group;
    QCOMPARE(s.root.hitElement(QPointF(80, 30)), groupItem);
    QCOMPARE(s.root.hitElement(QPointF(20, 10)), nullptr);
    ElementProps none = rect(0, 0, 100, 100, Qt::blue);
    none.responsible = true;
    none.pointerEvents = QStringLiteral("none");
    add(&s.root, none);
    QCOMPARE(s.root.hitElement(QPointF(80, 30)), groupItem);
    // A stroke-only shape is hit on its stroke and not in its hollow center.
    ElementProps ring = circle(50, 80, 15, Qt::black);
    ring.fill = Brush{};
    ring.stroke = color(Qt::black);
    ring.strokeWidth = px(6);
    ring.propList = {QStringLiteral("fill"), QStringLiteral("stroke"), QStringLiteral("strokeWidth")};
    ring.responsible = true;
    auto* ringItem = add(&s.root, ring);
    QCOMPARE(s.root.hitElement(QPointF(65, 80)), ringItem);
    QCOMPARE(s.root.hitElement(QPointF(50, 80)), nullptr);
  }

  void propUpdatesRepaintWithoutRemount() {
    Scene s;
    auto* item = add(&s.root, rect(0, 0, 50, 50, Qt::red));
    VERIFY_PX(render(s.root), 25, 25, Qt::red);
    const QPointer<RnsvgElementItem> before = item;
    ElementProps p = item->props();
    p.fill = color(Qt::blue);
    p.width = px(100);
    item->setProps(p);
    QCOMPARE(s.root.elementChildren().size(), 1);
    QCOMPARE(s.root.elementChildren().first(), before.data());
    const QImage img = render(s.root);
    VERIFY_PX(img, 25, 25, Qt::blue);
    VERIFY_PX(img, 75, 25, Qt::blue);
  }

  void insertRemoveAndOrderChangePaintOrder() {
    Scene s;
    auto* red = add(&s.root, rect(0, 0, 100, 100, Qt::red));
    auto* blue = add(&s.root, rect(0, 0, 100, 100, Qt::blue));
    VERIFY_PX(render(s.root), 50, 50, Qt::blue);
    blue->stackBefore(red);
    VERIFY_PX(render(s.root), 50, 50, Qt::red);
    red->setParentItem(nullptr);
    VERIFY_PX(render(s.root), 50, 50, Qt::blue);
    delete red;
    delete blue;
    VERIFY_TRANSPARENT(render(s.root), 50, 50);
  }

  void destroyingRootReleasesElements() {
    auto* root = new RnsvgRootItem;
    root->setSize(QSizeF(10, 10));
    auto* group = add(root, make(Kind::Group));
    QPointer<RnsvgElementItem> child = add(group, make(Kind::Path));
    QPointer<RnsvgElementItem> g = group;
    delete root;
    QVERIFY(g.isNull());
    QVERIFY(child.isNull());
  }

  void cyclicUseAndDeepNestingTerminate() {
    Scene s;
    ElementProps u = make(Kind::Use);
    u.name = QStringLiteral("self");
    u.href = QStringLiteral("self");
    add(&s.root, u);
    QQuickItem* parent = &s.root;
    for (int i = 0; i < 200; ++i) parent = add(parent, make(Kind::Group));
    add(parent, rect(0, 0, 100, 100, Qt::red));
    render(s.root);  // must return
    QVERIFY(true);
  }
};

QTEST_MAIN(RnsvgRenderTest)
#include "RnsvgRenderTest.moc"
