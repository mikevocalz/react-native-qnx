// RnsvgForeignObjectItem: placement from a scene-supplied transform, clipping
// to the box, immunity to Yoga's own position and size, and bounded behavior on
// hostile input. Pixels are read from a real software-rendered window.
#include "RnsvgForeignObjectItem.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTest>

#include <cmath>
#include <limits>

using rnqnx::views::RnsvgForeignObjectItem;

namespace {

// A live child that paints one solid color over its whole rectangle.
class Solid final : public QQuickPaintedItem {
 public:
  Solid(QColor color, QQuickItem* parent) : QQuickPaintedItem(parent), color_(color) {}
  void paint(QPainter* painter) override { painter->fillRect(QRectF(0, 0, width(), height()), color_); }

 private:
  QColor color_;
};

bool near(const QColor& a, const QColor& b) {
  return std::abs(a.red() - b.red()) <= 4 && std::abs(a.green() - b.green()) <= 4 && std::abs(a.blue() - b.blue()) <= 4;
}

}  // namespace

class RnsvgForeignObjectTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void hiddenUntilPlaced() {
    QQuickItem parent;
    RnsvgForeignObjectItem fo(&parent);
    parent.setVisible(true);
    QVERIFY(!fo.isVisible());
    QVERIFY(!fo.isPlaced());
    QVERIFY(fo.clip());
    fo.setPlacement(QTransform(), QSizeF(10, 10));
    QVERIFY(fo.isVisible());
    QVERIFY(fo.isPlaced());
    QCOMPARE(fo.size(), QSizeF(10, 10));
    fo.clearPlacement();
    QVERIFY(!fo.isVisible());
    QCOMPARE(fo.boxSize(), QSizeF());
  }

  void placementMapsBoxCoordinatesToTheParent() {
    QQuickItem parent;
    RnsvgForeignObjectItem fo(&parent);
    // Box point (u, v) -> parent point (20 + 2u, 30 + 2v).
    QTransform t;
    t.translate(20, 30);
    t.scale(2, 2);
    fo.setPlacement(t, QSizeF(50, 40));
    QCOMPARE(fo.mapToItem(&parent, QPointF(0, 0)), QPointF(20, 30));
    QCOMPARE(fo.mapToItem(&parent, QPointF(10, 5)), QPointF(40, 40));
    // Rotation by 90 degrees about the origin: (u, v) -> (-v, u) after translate.
    QTransform r;
    r.translate(100, 0);
    r.rotate(90);
    fo.setPlacement(r, QSizeF(50, 40));
    const QPointF p = fo.mapToItem(&parent, QPointF(10, 0));
    QVERIFY(std::abs(p.x() - 100) < 1e-4 && std::abs(p.y() - 10) < 1e-4);
  }

  void yogaPositionAndSizeDoNotMoveTheBox() {
    QQuickItem parent;
    RnsvgForeignObjectItem fo(&parent);
    QTransform t;
    t.translate(20, 30);
    fo.setPlacement(t, QSizeF(50, 40));
    // What the mounting layer does on every layout: x, y, width, height.
    fo.setX(5);
    fo.setY(70);  // a second ForeignObject stacked below another by Yoga
    fo.setWidth(300);
    fo.setHeight(400);
    QCOMPARE(fo.mapToItem(&parent, QPointF(0, 0)), QPointF(20, 30));
    QCOMPARE(fo.size(), QSizeF(50, 40));
  }

  void clipsLiveChildrenToTheBoxOnScreen() {
    QQuickWindow window;
    window.setColor(Qt::white);
    window.resize(200, 200);
    auto* fo = new RnsvgForeignObjectItem(window.contentItem());
    // The child is larger than the box and starts at its origin.
    new Solid(Qt::red, fo);
    auto* child = fo->childItems().first();
    child->setSize(QSizeF(120, 120));
    QTransform t;
    t.translate(40, 60);
    fo->setPlacement(t, QSizeF(50, 30));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const QImage image = window.grabWindow();
    QVERIFY(!image.isNull());
    const qreal dpr = image.devicePixelRatio();
    const auto at = [&](int x, int y) { return image.pixelColor(int(x * dpr), int(y * dpr)); };
    QVERIFY(near(at(60, 70), Qt::red));   // inside the box
    QVERIFY(near(at(42, 62), Qt::red));   // just inside the top-left corner
    QVERIFY(near(at(88, 88), Qt::red));   // just inside the bottom-right corner
    QVERIFY(near(at(95, 70), Qt::white)); // right of the box: the child is clipped
    QVERIFY(near(at(60, 95), Qt::white)); // below the box
    QVERIFY(near(at(30, 70), Qt::white)); // left of the box
  }

  void childrenOutsideTheBoxAreNotHit() {
    QQuickItem parent;
    parent.setSize(QSizeF(200, 200));
    RnsvgForeignObjectItem fo(&parent);
    auto* child = new QQuickItem(&fo);
    child->setSize(QSizeF(120, 120));
    QTransform t;
    t.translate(40, 60);
    fo.setPlacement(t, QSizeF(50, 30));
    QCOMPARE(parent.childAt(60, 70), &fo);
    QCOMPARE(fo.childAt(10, 10), child);
    QVERIFY(parent.childAt(120, 70) == nullptr);
  }

  void unusableInputHidesTheItem_data() {
    QTest::addColumn<QTransform>("transform");
    QTest::addColumn<QSizeF>("box");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    QTransform nanT;
    nanT.translate(nan, 0);
    QTransform infScale;
    infScale.scale(inf, 1);
    QTransform singular;
    singular.scale(0, 1);
    QTransform huge;
    huge.scale(1e300, 1e300);
    QTransform nearSingular;
    nearSingular.scale(1e-8, 1e-8);
    QTest::newRow("NaN translation") << nanT << QSizeF(10, 10);
    QTest::newRow("infinite scale") << infScale << QSizeF(10, 10);
    QTest::newRow("zero scale") << singular << QSizeF(10, 10);
    QTest::newRow("scale 1e300") << huge << QSizeF(10, 10);
    QTest::newRow("determinant 1e-16") << nearSingular << QSizeF(10, 10);
    QTest::newRow("zero width") << QTransform() << QSizeF(0, 10);
    QTest::newRow("zero height") << QTransform() << QSizeF(10, 0);
    QTest::newRow("negative width") << QTransform() << QSizeF(-5, 10);
    QTest::newRow("NaN width") << QTransform() << QSizeF(nan, 10);
    QTest::newRow("infinite height") << QTransform() << QSizeF(10, inf);
  }

  void unusableInputHidesTheItem() {
    QFETCH(QTransform, transform);
    QFETCH(QSizeF, box);
    QQuickItem parent;
    RnsvgForeignObjectItem fo(&parent);
    fo.setPlacement(QTransform(), QSizeF(10, 10));  // a good placement first
    QVERIFY(fo.isVisible());
    fo.setPlacement(transform, box);
    QVERIFY(!fo.isVisible());
    QVERIFY(!fo.isPlaced());
    // The next good placement recovers.
    fo.setPlacement(QTransform(), QSizeF(10, 10));
    QVERIFY(fo.isVisible());
  }

  void hugeBoxIsClamped() {
    QQuickItem parent;
    RnsvgForeignObjectItem fo(&parent);
    fo.setPlacement(QTransform(), QSizeF(1e300, 1e300));
    QVERIFY(fo.isPlaced());
    QCOMPARE(fo.size(), QSizeF(1e7, 1e7));
  }

  void deletingTheItemWithTheTransformAttachedIsSafe() {
    QQuickItem parent;
    auto* fo = new RnsvgForeignObjectItem(&parent);
    fo->setPlacement(QTransform::fromTranslate(3, 4), QSizeF(10, 10));
    delete fo;
    QVERIFY(parent.childItems().isEmpty());
  }
};

QTEST_MAIN(RnsvgForeignObjectTest)
#include "RnsvgForeignObjectTest.moc"
