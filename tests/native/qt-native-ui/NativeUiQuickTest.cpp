// Drives the drawer, tab bar and ExpoImage QML with Qt pointer and key events.
// A recorder QObject stands in for the Fabric bridge, so each test checks the
// calls the real bridge would turn into events for JavaScript.

#include <QDir>
#include <QImage>
#include <QObject>
#include <QAccessible>
#include <algorithm>
#include <functional>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStringList>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QtTest>

#include "react-native-qnx/views/QtCssColor.h"
#include "react-native-qnx/views/image/ExpoImageItem.h"

#include <memory>

namespace {

class Recorder final : public QObject {
  Q_OBJECT

 public:
  QStringList calls;

  Q_INVOKABLE void requestChange(bool open, const QString& reason) {
    calls << QStringLiteral("requestChange:%1:%2").arg(open ? "open" : "close", reason);
  }
  Q_INVOKABLE void transitionEnd(bool open) {
    calls << QStringLiteral("transitionEnd:%1").arg(open ? "open" : "closed");
  }
  Q_INVOKABLE void gestureStart(bool open) { calls << QStringLiteral("gestureStart:%1").arg(open ? "open" : "closed"); }
  Q_INVOKABLE void gestureEnd(bool open) { calls << QStringLiteral("gestureEnd:%1").arg(open ? "open" : "closed"); }
  Q_INVOKABLE void gestureCancel(bool open) { calls << QStringLiteral("gestureCancel:%1").arg(open ? "open" : "closed"); }
  Q_INVOKABLE void tabPressed(const QString& key) { calls << QStringLiteral("tabPressed:") + key; }
  Q_INVOKABLE void tabLongPressed(const QString& key) {
    calls << QStringLiteral("tabLongPressed:") + key;
  }
  Q_INVOKABLE void layoutModeChanged(const QString& mode, double size) {
    calls << QStringLiteral("layoutMode:%1:%2").arg(mode).arg(size);
  }
  Q_INVOKABLE QString resolveImage(const QString& uri) { return uri; }
  Q_INVOKABLE QVariant itemProperty(QObject* item, const QString& name) {
    return item ? item->property(name.toUtf8().constData()) : QVariant();
  }

  int count(const QString& prefix) const {
    int n = 0;
    for (const auto& call : calls) n += call.startsWith(prefix) ? 1 : 0;
    return n;
  }
};

struct Loaded {
  std::unique_ptr<QQuickItem> item;
  Recorder recorder;
};

class NativeUiQuickTest : public QObject {
  Q_OBJECT

 private:
  QQmlEngine engine_;
  std::unique_ptr<QQuickWindow> window_;

  std::unique_ptr<QQuickItem> load(const QString& name, const QSize& size) {
    QQmlComponent component(&engine_, QUrl::fromLocalFile(QStringLiteral(RNQNX_NATIVE_UI_QML_DIR "/") + name));
    if (component.isError()) {
      qWarning().noquote() << component.errorString();
      return nullptr;
    }
    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem*>(component.create()));
    if (!item) return nullptr;
    item->setSize(size);
    item->setParentItem(window_->contentItem());
    return item;
  }

  // What the mounting layer does: a child lands on `contentItem` when the item has one.
  QQuickItem* mountChild(QQuickItem* parent, const QRectF& frame) {
    auto* child = new QQuickItem();
    QQuickItem* target = parent->property("contentItem").value<QQuickItem*>();
    child->setParentItem(target != nullptr ? target : parent);
    child->setPosition(frame.topLeft());
    child->setSize(frame.size());
    return child;
  }

  static bool settled(QQuickItem* drawer, double progress) {
    return qFuzzyCompare(drawer->property("progress").toDouble() + 1.0, progress + 1.0);
  }


  // A panel item the way the mounting layer exposes a Pressable: an accessible
  // button with a press action. `presses` counts the accessibility press actions.
  QQuickItem* mountNavItem(QQuickItem* panel, const QString& label, const QRectF& frame, bool selected = false) {
    QQmlComponent component(&engine_);
    component.setData(
        "import QtQuick\n"
        "Item {\n"
        "  property bool accessible: true\n"
        "  property string accessibilityRole: 'button'\n"
        "  property string accessibilityLabel: ''\n"
        "  property bool accessibilityStateSelected: false\n"
        "  property bool accessibilityStateDisabled: false\n"
        "  property real radius: 8\n"
        "  property int presses: 0\n"
        "  Accessible.role: Accessible.Button\n"
        "  Accessible.name: accessibilityLabel\n"
        "  Accessible.onPressAction: presses++\n"
        "}\n",
        QUrl());
    auto* item = qobject_cast<QQuickItem*>(component.create());
    if (item == nullptr) {
      qWarning().noquote() << component.errorString();
      return nullptr;
    }
    item->setProperty("accessibilityLabel", label);
    item->setProperty("accessibilityStateSelected", selected);
    item->setParentItem(panel);
    item->setPosition(frame.topLeft());
    item->setSize(frame.size());
    return item;
  }

  static QQuickItem* layer(QQuickItem* drawer, const char* name) {
    return drawer->findChild<QQuickItem*>(QString::fromLatin1(name));
  }

  // The window point at the middle of `item`.
  QPoint centerOf(QQuickItem* item) const {
    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
  }

  // A drawer with a 280 px panel of three nav items (y 20, 70, 120) and the content.
  struct DrawerFixture {
    std::unique_ptr<QQuickItem> drawer;
    Recorder recorder;
    QQuickItem* panel = nullptr;
    QQuickItem* main = nullptr;
    QList<QQuickItem*> items;
  };
  void openedDrawerWithItems(DrawerFixture& fx) {
    fx.drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(fx.drawer);
    fx.drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&fx.recorder)));
    fx.panel = mountChild(fx.drawer.get(), QRectF(0, 0, 280, 400));
    fx.main = mountChild(fx.drawer.get(), QRectF(0, 0, 600, 400));
    const QStringList labels = {QStringLiteral("Home"), QStringLiteral("Media"), QStringLiteral("About")};
    for (int i = 0; i < labels.size(); ++i) {
      fx.items << mountNavItem(fx.panel, labels[i], QRectF(10, 20 + 50 * i, 260, 40), i == 1);
      QVERIFY(fx.items.last());
    }
    QVERIFY(QTest::qWaitFor([&] { return fx.drawer->property("ready").toBool(); }));
    fx.drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(fx.drawer.get(), 1.0); }, 2000));
  }

  // A deliberate drag: slow enough that its release velocity is not a fling.
  void drag(const QPoint& from, const QPoint& to, int stepMs = 60) {
    QTest::mousePress(window_.get(), Qt::LeftButton, Qt::NoModifier, from);
    const int steps = 8;
    for (int i = 1; i <= steps; ++i) {
      QTest::mouseMove(window_.get(), from + (to - from) * i / steps);
      QTest::qWait(stepMs);
    }
    QTest::qWait(120);  // rest before release so the release velocity is ~0
    QTest::mouseRelease(window_.get(), Qt::LeftButton, Qt::NoModifier, to);
  }

 private Q_SLOTS:
  void initTestCase() {
    rnqnx::views::ExpoImageItem::registerQmlType();
    window_ = std::make_unique<QQuickWindow>();
    window_->resize(800, 500);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
  }
  void cleanupTestCase() { window_.reset(); }

  // ---- color strings ----------------------------------------------------

  void cssColorsParseTheWayJavaScriptWritesThem() {
    using rnqnx::views::parseCssColor;
    QCOMPARE(*parseCssColor(QStringLiteral("rgba(0, 0, 0, 0.5)")), QColor(0, 0, 0, 128));
    QCOMPARE(*parseCssColor(QStringLiteral("rgb(255 128 0)")), QColor(255, 128, 0, 255));
    QCOMPARE(*parseCssColor(QStringLiteral("rgba(10,20,30,50%)")), QColor(10, 20, 30, 128));
    QCOMPARE(*parseCssColor(QStringLiteral("#fff")), QColor(255, 255, 255, 255));
    QCOMPARE(*parseCssColor(QStringLiteral("#0008")), QColor(0, 0, 0, 136));
    QCOMPARE(*parseCssColor(QStringLiteral("#11223380")), QColor(0x11, 0x22, 0x33, 0x80));  // alpha last
    QCOMPARE(*parseCssColor(QStringLiteral(" red ")), QColor(255, 0, 0));
    QVERIFY(!parseCssColor(QStringLiteral("#12")));
    QVERIFY(!parseCssColor(QStringLiteral("#ggg")));
    QVERIFY(!parseCssColor(QStringLiteral("rgba(1,2)")));
    QVERIFY(!parseCssColor(QStringLiteral("rgb(a,b,c)")));
    QVERIFY(!parseCssColor(QStringLiteral("not-a-color")));
  }

  // ---- drawer -----------------------------------------------------------

  void drawerOpensAndClosesFromProps() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    auto* panel = mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    auto* main = mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));
    QCOMPARE(drawer->property("drawerPanel").value<QQuickItem*>(), panel);
    QCOMPARE(drawer->property("mainPanel").value<QQuickItem*>(), main);
    QVERIFY(!panel->isVisible());  // closed: nothing to paint or hit
    QCOMPARE(panel->z(), 2.0);

    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("transitionEnd:open")) == 1; }));
    QVERIFY(panel->isVisible());
    // A change from JavaScript is never reported as a request.
    QCOMPARE(recorder.count(QStringLiteral("requestChange")), 0);

    drawer->setProperty("open", false);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 0.0); }, 2000));
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("transitionEnd:closed")) == 1; }));
    QVERIFY(!panel->isVisible());
  }

  // The mounting layer appends a child and then restacks it with stackBefore,
  // so the panel can arrive after the content and still be child 0.
  void drawerPanelInsertedBeforeContentKeepsItsRole() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    auto* main = mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    auto* panel = mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    panel->stackBefore(main);
    QMetaObject::invokeMethod(drawer.get(), "syncPanels");
    QCOMPARE(drawer->property("drawerPanel").value<QQuickItem*>(), panel);
    QCOMPARE(drawer->property("mainPanel").value<QQuickItem*>(), main);
  }

  void drawerStartsOpenWithoutAnimating() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    drawer->setProperty("open", true);  // as the first props of a new view
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));
    QVERIFY(settled(drawer.get(), 1.0));
  }

  void scrimCoversMainAndClosesOnTap() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    auto* main = mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));

    auto* scrim = drawer->findChild<QQuickItem*>(QStringLiteral("rnqnxDrawerScrim"));
    QVERIFY(scrim);
    QCOMPARE(scrim->parentItem(), main);  // the dispatcher's hit test finds it there
    QVERIFY(scrim->isVisible());
    QVERIFY(scrim->z() > 1000);

    const QPoint onMain = drawer->mapToScene(QPointF(450, 200)).toPoint();
    QTest::mouseClick(window_.get(), Qt::LeftButton, Qt::NoModifier, onMain);
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("requestChange:close:scrim")) == 1; }));
  }

  void escapeRequestsCloseAndRevertsWhenRefused() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));
    drawer->forceActiveFocus();

    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("requestChange:close:escape")) == 1; }));
    // JavaScript keeps `open` true. The drawer returns to it.
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 3000));
  }

  void edgeSwipeOpensPastFortyPercent() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    drawer->setProperty("flingVelocity", 1.0e9);  // QTest's synthetic timestamps give no real velocity
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));

    // Too short: 40 px of 280 is under 40%, so it falls back without a request.
    drag(QPoint(5, 200), QPoint(45, 200));
    QTest::qWait(300);
    QCOMPARE(recorder.count(QStringLiteral("requestChange")), 0);
    QVERIFY(settled(drawer.get(), 0.0));

    drag(QPoint(5, 200), QPoint(220, 200));
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("requestChange:open:swipe")) == 1; }));
    // JavaScript confirms.
    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));
  }

  void edgeSwipeStaysOutOfTheRail() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    drawer->setProperty("swipeExclusionLeft", 88.0);
    drawer->setProperty("flingVelocity", 1.0e9);
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));

    drag(QPoint(10, 200), QPoint(260, 200));  // starts over the rail
    QTest::qWait(250);
    QCOMPARE(recorder.count(QStringLiteral("requestChange")), 0);

    drag(QPoint(92, 200), QPoint(340, 200));  // starts just past it
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("requestChange:open:swipe")) == 1; }));
  }

  void quickFlingOpensEvenWhenShort() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));
    QTest::mousePress(window_.get(), Qt::LeftButton, Qt::NoModifier, QPoint(5, 200));
    for (int i = 1; i <= 6; ++i) {
      QTest::mouseMove(window_.get(), QPoint(5 + i * 12, 200));
      QTest::qWait(10);
    }
    QTest::mouseRelease(window_.get(), Qt::LeftButton, Qt::NoModifier, QPoint(77, 200));
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("requestChange:open:swipe")) == 1; }));
  }

  void swipeDisabledIgnoresDrags() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    drawer->setProperty("swipeEnabled", false);
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));
    drag(QPoint(5, 200), QPoint(260, 200));
    QTest::qWait(250);
    QCOMPARE(recorder.count(QStringLiteral("requestChange")), 0);
  }

  void rightDrawerSlidesInFromTheRightAndSlideModePushesContent() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    drawer->setProperty("side", QStringLiteral("right"));
    drawer->setProperty("drawerType", QStringLiteral("slide"));
    auto* panel = mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    auto* main = mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));
    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));
    // Open on the right: the panel's right edge is at the container's right edge.
    QCOMPARE(panel->mapToItem(drawer.get(), QPointF(0, 0)).x(), 600.0 - 280.0);
    // Slide mode: the content moved 280 px away from the drawer.
    QCOMPARE(main->mapToItem(drawer.get(), QPointF(0, 0)).x(), -280.0);
  }

  void backDrawerKeepsThePanelAndSlidesTheContentAway() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    drawer->setProperty("drawerType", QStringLiteral("back"));
    auto* panel = mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    auto* main = mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));
    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));
    QCOMPARE(panel->mapToItem(drawer.get(), QPointF(0, 0)).x(), 0.0);   // the panel never moves
    QCOMPARE(main->mapToItem(drawer.get(), QPointF(0, 0)).x(), 280.0);  // the content slides away
    QVERIFY(panel->z() < main->z());                                    // and the panel is under it
    drawer->setProperty("open", false);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 0.0); }, 2000));
    QCOMPARE(main->mapToItem(drawer.get(), QPointF(0, 0)).x(), 0.0);
  }


  void drawerReadsDynamicMountMetadata() {
    const auto resetPointer = qScopeGuard([&] {
      QTest::mouseMove(window_.get(), QPoint(790, 490));
      QCoreApplication::processEvents();
    });
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    auto* panel = mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    // Real generic Fabric items receive QObject dynamic properties. Unlike the
    // older fixture these are not declared QML properties.
    auto* nav = mountChild(panel, QRectF(10, 20, 260, 40));
    nav->setProperty("accessible", true);
    nav->setProperty("accessibilityRole", QStringLiteral("button"));
    nav->setProperty("accessibilityStateSelected", true);
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));
    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));
    QCOMPARE(drawer->property("focusedIndex").toInt(), 0);
    QTest::mouseMove(window_.get(), centerOf(nav));
    QVERIFY(QTest::qWaitFor([&] { return layer(drawer.get(), "rnqnxDrawerItemState")->isVisible(); }));
    QTest::keyClick(window_.get(), Qt::Key_Down);
    QVERIFY(layer(drawer.get(), "rnqnxDrawerFocusRing")->isVisible());
    nav->setProperty("accessibilityStateDisabled", true);
    QMetaObject::invokeMethod(drawer.get(), "collectNavItems");
    QCOMPARE(drawer->property("focusedIndex").toInt(), -1);
  }

  void drawerItemsShowHoverAndPressedStates() {
    DrawerFixture fx;
    openedDrawerWithItems(fx);
    if (QTest::currentTestFailed()) return;
    auto* state = layer(fx.drawer.get(), "rnqnxDrawerItemState");
    QVERIFY(state);
    QVERIFY(state->property("rnqnxInputTransparent").toBool());
    QVERIFY(layer(fx.drawer.get(), "rnqnxDrawerFocusRing")->property("rnqnxInputTransparent").toBool());
    QVERIFY(!state->isVisible());

    QTest::mouseMove(window_.get(), centerOf(fx.items[1]));
    QVERIFY(QTest::qWaitFor([&] { return state->isVisible(); }));
    QCOMPARE(state->parentItem(), fx.panel);
    QCOMPARE(state->width(), 260.0);
    QCOMPARE(state->height(), 40.0);
    QCOMPARE(state->mapToItem(fx.panel, QPointF(0, 0)), QPointF(10, 70));
    QCOMPARE(state->property("radius").toDouble(), 8.0);
    QCOMPARE(state->property("color").value<QColor>(), QColor::fromRgba(0x22ffffff));
    QCOMPARE(fx.drawer->property("hoveredItem").value<QQuickItem*>(), fx.items[1]);

    // Moving to another item moves the layer.
    QTest::mouseMove(window_.get(), centerOf(fx.items[2]));
    QVERIFY(QTest::qWaitFor([&] { return state->mapToItem(fx.panel, QPointF(0, 0)).y() == 120.0; }));

    // Pressed is a stronger layer, and it ends with the button.
    QTest::mousePress(window_.get(), Qt::LeftButton, Qt::NoModifier, centerOf(fx.items[2]));
    QVERIFY(QTest::qWaitFor([&] { return fx.drawer->property("pressedItem").value<QQuickItem*>() == fx.items[2]; }));
    QTRY_COMPARE(state->property("color").value<QColor>(), QColor::fromRgba(0x33ffffff));
    QTest::mouseRelease(window_.get(), Qt::LeftButton, Qt::NoModifier, centerOf(fx.items[2]));
    QVERIFY(QTest::qWaitFor([&] { return fx.drawer->property("pressedItem").value<QQuickItem*>() == nullptr; }));
    QTRY_COMPARE(state->property("color").value<QColor>(), QColor::fromRgba(0x22ffffff));

    // Off the items: no layer.
    QTest::mouseMove(window_.get(), fx.drawer->mapToScene(QPointF(150, 300)).toPoint());
    QVERIFY(QTest::qWaitFor([&] { return !state->isVisible(); }));
  }

  void drawerKeyboardMovesFocusAndPressesItems() {
    DrawerFixture fx;
    openedDrawerWithItems(fx);
    if (QTest::currentTestFailed()) return;
    QVERIFY(fx.drawer->hasActiveFocus());
    auto* ring = layer(fx.drawer.get(), "rnqnxDrawerFocusRing");
    QVERIFY(ring);
    QVERIFY(!ring->isVisible());  // no keyboard use yet
    auto ringIndex = [&] {
      return ring->isVisible() ? qRound((ring->y() - 20) / 50) : -1;
    };

    // The drawer starts on the selected item (Media); the first arrow moves from it.
    QCOMPARE(fx.drawer->property("focusedIndex").toInt(), 1);
    QTest::keyClick(window_.get(), Qt::Key_Down);
    QCOMPARE(ringIndex(), 2);
    QCOMPARE(ring->width(), 260.0);
    QTest::keyClick(window_.get(), Qt::Key_Down);  // wraps
    QCOMPARE(ringIndex(), 0);
    QTest::keyClick(window_.get(), Qt::Key_Up);    // wraps back
    QCOMPARE(ringIndex(), 2);
    QTest::keyClick(window_.get(), Qt::Key_Home);
    QCOMPARE(ringIndex(), 0);
    QTest::keyClick(window_.get(), Qt::Key_End);
    QCOMPARE(ringIndex(), 2);
    QTest::keyClick(window_.get(), Qt::Key_Tab);   // Tab stays in the panel
    QCOMPARE(ringIndex(), 0);
    QTest::keyClick(window_.get(), Qt::Key_Tab, Qt::ShiftModifier);
    QCOMPARE(ringIndex(), 2);
    QVERIFY(fx.drawer->hasActiveFocus());

    QTest::keyClick(window_.get(), Qt::Key_Return);
    QCOMPARE(fx.items[2]->property("presses").toInt(), 1);
    QTest::keyClick(window_.get(), Qt::Key_Home);
    QTest::keyClick(window_.get(), Qt::Key_Space);
    QTest::keyClick(window_.get(), Qt::Key_Enter);
    QCOMPARE(fx.items[0]->property("presses").toInt(), 2);
    QCOMPARE(fx.items[1]->property("presses").toInt(), 0);

    // Escape asks to close.
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QVERIFY(QTest::qWaitFor([&] { return fx.recorder.count(QStringLiteral("requestChange:close:escape")) == 1; }));
  }

  void drawerSkipsDisabledAndHiddenItems() {
    DrawerFixture fx;
    openedDrawerWithItems(fx);
    if (QTest::currentTestFailed()) return;
    fx.items[1]->setProperty("accessibilityStateDisabled", true);
    fx.items[2]->setVisible(false);
    QTest::keyClick(window_.get(), Qt::Key_Home);
    QTest::keyClick(window_.get(), Qt::Key_Down);  // only Home is left
    QCOMPARE(fx.drawer->property("focusedIndex").toInt(), 0);
    QCOMPARE(fx.drawer->property("navItems").value<QJSValue>().property(QStringLiteral("length")).toInt(), 1);
    // A pressable view that is not a button is not a nav item.
    fx.items[0]->setProperty("accessibilityRole", QStringLiteral("text"));
    QTest::keyClick(window_.get(), Qt::Key_End);
    QCOMPARE(fx.drawer->property("navItems").value<QJSValue>().property(QStringLiteral("length")).toInt(), 0);
  }

  void drawerHoldsFocusWhileOpenAndGivesItBack() {
    auto* opener = new QQuickItem(window_->contentItem());
    opener->setSize(QSizeF(20, 20));
    opener->setObjectName(QStringLiteral("opener"));
    auto* other = new QQuickItem(window_->contentItem());
    other->setSize(QSizeF(20, 20));
    opener->forceActiveFocus();
    QCOMPARE(window_->activeFocusItem(), opener);

    DrawerFixture fx;
    fx.drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(fx.drawer);
    fx.drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&fx.recorder)));
    mountChild(fx.drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(fx.drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return fx.drawer->property("ready").toBool(); }));
    QCOMPARE(window_->activeFocusItem(), opener);  // a closed drawer does not take focus

    fx.drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(fx.drawer.get(), 1.0); }, 2000));
    QCOMPARE(window_->activeFocusItem(), fx.drawer.get());
    QCOMPARE(fx.drawer->property("opener").value<QQuickItem*>(), opener);

    // Focus trap: something else asking for focus loses it.
    other->forceActiveFocus();
    QVERIFY(QTest::qWaitFor([&] { return window_->activeFocusItem() == fx.drawer.get(); }));

    fx.drawer->setProperty("open", false);
    QVERIFY(QTest::qWaitFor([&] { return settled(fx.drawer.get(), 0.0); }, 2000));
    QVERIFY(QTest::qWaitFor([&] { return window_->activeFocusItem() == opener; }));
    // Closed again: focus is free to move.
    other->forceActiveFocus();
    QCOMPARE(window_->activeFocusItem(), other);
    fx.drawer.reset();
    delete opener;
    delete other;
  }

  void swipeEdgeWidthSetsTheStartStrip() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    drawer->setProperty("flingVelocity", 1.0e9);
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));

    // Default 24 px: a drag from x = 50 starts outside the strip.
    drag(QPoint(50, 200), QPoint(270, 200));
    QTest::qWait(250);
    QCOMPARE(recorder.count(QStringLiteral("requestChange")), 0);

    drawer->setProperty("edgeWidth", 80.0);
    drag(QPoint(50, 200), QPoint(270, 200));
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("requestChange:open:swipe")) == 1; }));

    // A narrow strip excludes what the default would have taken.
    drawer->setProperty("open", false);
    drawer->setProperty("progress", 0.0);
    recorder.calls.clear();
    drawer->setProperty("edgeWidth", 4.0);
    drag(QPoint(12, 200), QPoint(240, 200));
    QTest::qWait(250);
    QCOMPARE(recorder.count(QStringLiteral("requestChange")), 0);
  }

  void rightDrawerOpensFromTheRightEdgeStrip() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    drawer->setProperty("side", QStringLiteral("right"));
    drawer->setProperty("edgeWidth", 40.0);
    drawer->setProperty("flingVelocity", 1.0e9);
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));

    drag(QPoint(10, 200), QPoint(240, 200));  // the left edge is not the right drawer's strip
    QTest::qWait(250);
    QCOMPARE(recorder.count(QStringLiteral("requestChange")), 0);

    drag(QPoint(570, 200), QPoint(350, 200));  // inside the 40 px right strip, dragged left
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("requestChange:open:swipe")) == 1; }));
  }

  void overlayColorIsTheScrimColor() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    drawer->setProperty("scrimColor", QColor(0x12, 0x34, 0x56, 0x78));
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));
    auto* scrim = drawer->findChild<QQuickItem*>(QStringLiteral("rnqnxDrawerScrim"));
    QVERIFY(scrim);
    QCOMPARE(scrim->property("color").value<QColor>(), QColor(0x12, 0x34, 0x56, 0x78));
    drawer->setProperty("scrimColor", QColor(Qt::transparent));
    QCOMPARE(scrim->property("color").value<QColor>().alpha(), 0);
    // A transparent scrim still closes the drawer on tap (the hit area stays).
    QVERIFY(scrim->isVisible());
  }

  void frontDrawerLaysOverTheContentWithoutMovingIt() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    drawer->setProperty("drawerType", QStringLiteral("front"));
    auto* panel = mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    auto* main = mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));
    QCOMPARE(panel->mapToItem(drawer.get(), QPointF(0, 0)).x(), -280.0);  // parked off screen
    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));
    QCOMPARE(panel->mapToItem(drawer.get(), QPointF(0, 0)).x(), 0.0);
    QCOMPARE(main->mapToItem(drawer.get(), QPointF(0, 0)).x(), 0.0);  // the content stays put
    QVERIFY(panel->z() > main->z());                                  // and the panel is over it
  }

  void drawerWidthMovesThePanelAndTheScrim() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    drawer->setProperty("drawerWidth", 200.0);
    drawer->setProperty("drawerType", QStringLiteral("slide"));
    auto* panel = mountChild(drawer.get(), QRectF(0, 0, 200, 400));
    auto* main = mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));
    QCOMPARE(panel->mapToItem(drawer.get(), QPointF(0, 0)).x(), -200.0);
    drawer->setProperty("open", true);
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 1.0); }, 2000));
    QCOMPARE(panel->mapToItem(drawer.get(), QPointF(0, 0)).x(), 0.0);
    QCOMPARE(main->mapToItem(drawer.get(), QPointF(0, 0)).x(), 200.0);
  }


  void swipeMinDistanceRejectsShortDragsThatPassTheFortyPercentRule() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    drawer->setProperty("flingVelocity", 1.0e9);
    drawer->setProperty("drawerWidth", 100.0);  // 50 px is 50% of the panel
    drawer->setProperty("edgeWidth", 100.0);
    mountChild(drawer.get(), QRectF(0, 0, 100, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));

    drawer->setProperty("swipeMinDistance", 80.0);
    drag(QPoint(5, 200), QPoint(60, 200));  // 55 px: over 40% of 100, under 80 px
    QTest::qWait(250);
    QCOMPARE(recorder.count(QStringLiteral("requestChange")), 0);
    QVERIFY(settled(drawer.get(), 0.0));

    drawer->setProperty("swipeMinDistance", 30.0);
    drag(QPoint(5, 200), QPoint(60, 200));
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("requestChange:open:swipe")) == 1; }));
  }

  void gestureEventsBracketADragAndCancelSettlesBack() {
    auto drawer = load(QStringLiteral("QtDrawer.qml"), QSize(600, 400));
    QVERIFY(drawer);
    Recorder recorder;
    drawer->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    drawer->setProperty("flingVelocity", 1.0e9);
    mountChild(drawer.get(), QRectF(0, 0, 280, 400));
    mountChild(drawer.get(), QRectF(0, 0, 600, 400));
    QVERIFY(QTest::qWaitFor([&] { return drawer->property("ready").toBool(); }));

    drag(QPoint(5, 200), QPoint(220, 200));
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("requestChange:open:swipe")) == 1; }));
    QCOMPARE(recorder.calls.indexOf(QStringLiteral("gestureStart:closed")), 0);
    QVERIFY(recorder.calls.indexOf(QStringLiteral("gestureEnd:closed")) > 0);
    QCOMPARE(recorder.count(QStringLiteral("gestureCancel")), 0);

    // A drag the system takes away reports a cancel and goes back to `open`.
    recorder.calls.clear();
    drawer->setProperty("open", false);
    drawer->setProperty("progress", 0.0);
    QMetaObject::invokeMethod(drawer.get(), "beginDrag");
    drawer->setProperty("progress", 0.6);
    QMetaObject::invokeMethod(drawer.get(), "cancelDrag");
    QCOMPARE(recorder.calls, (QStringList{QStringLiteral("gestureStart:closed"), QStringLiteral("gestureCancel:closed")}));
    QVERIFY(QTest::qWaitFor([&] { return settled(drawer.get(), 0.0); }, 2000));
    QCOMPARE(recorder.count(QStringLiteral("requestChange")), 0);
  }

  // ---- tab bar ----------------------------------------------------------

  QVariantList tabs() const {
    QVariantList list;
    for (const char* key : {"home", "search", "inbox"}) {
      QVariantMap tab;
      tab.insert(QStringLiteral("key"), QString::fromLatin1(key));
      tab.insert(QStringLiteral("title"), QString::fromLatin1(key).toUpper());
      tab.insert(QStringLiteral("icon"), QStringLiteral("*"));
      tab.insert(QStringLiteral("badge"), QString::fromLatin1(key) == "inbox" ? QStringLiteral("3") : QString());
      list.append(tab);
    }
    return list;
  }

  void tabBarSwitchesModeAndReportsSize() {
    auto bar = load(QStringLiteral("QtTabBar.qml"), QSize(800, 60));
    QVERIFY(bar);
    Recorder recorder;
    bar->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    bar->setProperty("tabs", tabs());
    bar->setProperty("activeKey", QStringLiteral("home"));
    bar->setProperty("availableWidth", 900.0);
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("layoutMode:")) >= 1; }));
    QCOMPARE(recorder.calls.last(), QStringLiteral("layoutMode:side:88"));

    bar->setProperty("availableWidth", 500.0);  // a window resize
    QVERIFY(QTest::qWaitFor([&] { return recorder.calls.last() == QStringLiteral("layoutMode:bottom:60"); }));

    bar->setProperty("layout", QStringLiteral("side"));  // forced, whatever the width
    QVERIFY(QTest::qWaitFor([&] { return recorder.calls.last() == QStringLiteral("layoutMode:side:88"); }));

    bar->setProperty("hidden", true);
    QVERIFY(QTest::qWaitFor([&] { return recorder.calls.last() == QStringLiteral("layoutMode:side:0"); }));
    QVERIFY(!bar->isVisible());
  }

  // The content beside a left rail is the window minus the rail, so a card
  // padded 28 px from the content's right edge ends 28 px inside the window
  // with its corners rounded. Golden pixels at the right edge, at narrow,
  // normal and wide windows, and after a resize.
  void contentBesideTheRailEndsInsideTheWindowWithRoundedCorners() {
    auto bar = load(QStringLiteral("QtTabBar.qml"), QSize(96, 400));
    QVERIFY(bar);
    Recorder recorder;
    bar->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    bar->setProperty("tabs", tabs());
    bar->setProperty("activeKey", QStringLiteral("home"));
    bar->setProperty("layout", QStringLiteral("side"));
    bar->setProperty("railWidth", 96.0);
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("layoutMode:side:96")) >= 1; }));
    const double rail = bar->property("occupied").toDouble();
    QCOMPARE(rail, 96.0);

    auto* content = new QQuickItem(window_->contentItem());
    auto* card = new QQuickItem(content);
    const QColor background(12, 25, 41);
    const QColor cardColor(20, 37, 56);
    // Two plain items stand in for the page and its rounded card.
    const auto paint = [&](QQuickItem* item, const QColor& color, qreal radius) {
      QQmlComponent component(&engine_);
      component.setData(QByteArrayLiteral("import QtQuick\nRectangle { }"), QUrl());
      auto* rect = qobject_cast<QQuickItem*>(component.create());
      QVERIFY(rect);
      rect->setParentItem(item);
      rect->setZ(-1);  // behind the item's other children
      rect->setProperty("color", color);
      rect->setProperty("radius", radius);
      rect->setWidth(item->width());
      rect->setHeight(item->height());
      QObject::connect(item, &QQuickItem::widthChanged, rect, [item, rect] { rect->setWidth(item->width()); });
      QObject::connect(item, &QQuickItem::heightChanged, rect, [item, rect] { rect->setHeight(item->height()); });
    };
    content->setSize(QSizeF(1, 1));
    paint(content, background, 0);
    paint(card, cardColor, 14);

    for (const int windowWidth : {360, 586, 880, 1280}) {
      window_->resize(windowWidth, 400);
      QVERIFY(QTest::qWaitFor([&] { return window_->width() == windowWidth; }));
      bar->setSize(QSizeF(rail, 400));
      // What the router's layout does: content starts where the rail ends.
      content->setPosition(QPointF(rail, 0));
      content->setSize(QSizeF(windowWidth - rail, 400));
      card->setPosition(QPointF(28, 18));
      card->setSize(QSizeF(content->width() - 56, 200));
      QTest::qWait(60);
      const QImage frame = window_->grabWindow();
      QVERIFY(!frame.isNull());
      const int right = windowWidth - 28;       // the card's right edge
      const int midY = 18 + 100;
      QCOMPARE(frame.pixelColor(right - 2, midY), cardColor);       // card reaches its edge
      QCOMPARE(frame.pixelColor(right + 2, midY), background);      // and stops there: 28 px of padding
      QCOMPARE(frame.pixelColor(windowWidth - 2, midY), background);
      // The corner is rounded: the card's own color is absent at the very
      // corner pixel and present a radius inwards.
      QCOMPARE(frame.pixelColor(right - 1, 18), background);
      QCOMPARE(frame.pixelColor(right - 14, 18 + 1), cardColor);
      // The left edge is not shifted by the rail either.
      QCOMPARE(frame.pixelColor(static_cast<int>(rail) + 28 + 2, midY), cardColor);
    }
    window_->resize(800, 500);
  }

  void tabBarEdgesRtlAndLabels() {
    auto bar = load(QStringLiteral("QtTabBar.qml"), QSize(600, 60));
    QVERIFY(bar);
    Recorder recorder;
    bar->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    bar->setProperty("tabs", tabs());
    bar->setProperty("activeKey", QStringLiteral("home"));
    bar->setProperty("dividerColor", QColor(255, 0, 0));
    const auto tabItem = [&](int index) -> QQuickItem* {
      // The Repeater's delegates are the direct children that carry Accessible.PageTab.
      QList<QQuickItem*> found;
      for (auto* child : bar->childItems())
        if (child->property("active").isValid()) found.append(child);
      std::sort(found.begin(), found.end(), [](QQuickItem* a, QQuickItem* b) { return a->property("index").toInt() < b->property("index").toInt(); });
      return index < found.size() ? found.at(index) : nullptr;
    };

    QVERIFY(QTest::qWaitFor([&] { return tabItem(2) != nullptr; }));
    // top: a horizontal bar whose divider is on its bottom row.
    bar->setProperty("position", QStringLiteral("top"));
    QVERIFY(QTest::qWaitFor([&] { return !recorder.calls.isEmpty() && recorder.calls.last() == QStringLiteral("layoutMode:bottom:60"); }));
    QCOMPARE(bar->property("edge").toString(), QStringLiteral("top"));
    QVERIFY(tabItem(0)->x() < tabItem(1)->x());

    // right: a rail whose divider is the left column.
    bar->setSize(QSizeF(96, 400));
    bar->setProperty("position", QStringLiteral("right"));
    QVERIFY(QTest::qWaitFor([&] { return !recorder.calls.isEmpty() && recorder.calls.last() == QStringLiteral("layoutMode:side:88"); }));
    QCOMPARE(bar->property("railOnRight").toBool(), true);

    // left in right-to-left sits on the physical right, and horizontal tabs run right to left.
    bar->setProperty("position", QStringLiteral("left"));
    bar->setProperty("rtl", true);
    QCOMPARE(bar->property("railOnRight").toBool(), true);
    bar->setProperty("position", QStringLiteral("bottom"));
    bar->setSize(QSizeF(600, 60));
    QVERIFY(QTest::qWaitFor([&] { return tabItem(2) && tabItem(0)->x() > tabItem(1)->x(); }));
    bar->setProperty("rtl", false);

    // showLabel false removes every title.
    const auto visibleTitles = [&] {
      int n = 0;
      std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
        if (item->property("text").toString() == QStringLiteral("HOME") && item->isVisible()) ++n;
        for (auto* child : item->childItems()) walk(child);
      };
      walk(bar.get());
      return n;
    };
    QCOMPARE(visibleTitles(), 1);
    bar->setProperty("showLabel", false);
    QCOMPARE(visibleTitles(), 0);
  }

  // A held press has to stand out from a resting tab on the bar's own
  // background, light or dark. The wash used to be fixed white, which a light
  // bar hid completely.
  void tabBarPressedStateShowsOnLightAndDarkBars() {
    const struct {
      QColor background;
      QColor tint;
    } bars[] = {{QColor(255, 255, 255), QColor(40, 48, 60)}, {QColor(0, 0, 0), QColor(220, 228, 240)}};
    for (const auto& scheme : bars) {
      auto bar = load(QStringLiteral("QtTabBar.qml"), QSize(600, 60));
      QVERIFY(bar);
      bar->setProperty("tabs", tabs());
      bar->setProperty("activeKey", QStringLiteral("home"));
      bar->setProperty("layout", QStringLiteral("bottom"));
      bar->setProperty("backgroundColor", scheme.background);
      bar->setProperty("tintInactive", scheme.tint);
      QTest::qWait(50);
      const QPoint second(300, 30);  // the middle tab, inactive
      const QColor before = window_->grabWindow().pixelColor(second.x() - 40, 8);
      QTest::mousePress(window_.get(), Qt::LeftButton, Qt::NoModifier, second);
      QTest::qWait(1000);  // held, as a long finger or button press is
      const QColor held = window_->grabWindow().pixelColor(second.x() - 40, 8);
      const int delta = std::abs(held.red() - before.red()) + std::abs(held.green() - before.green()) +
                        std::abs(held.blue() - before.blue());
      QTest::mouseRelease(window_.get(), Qt::LeftButton, Qt::NoModifier, second);
      QVERIFY2(delta >= 30, qPrintable(QStringLiteral("held tab differs by only %1 on %2")
                                           .arg(delta)
                                           .arg(scheme.background.name())));
    }
  }

  void tabBarPressedStateAndEscape() {
    auto bar = load(QStringLiteral("QtTabBar.qml"), QSize(600, 60));
    QVERIFY(bar);
    bar->setProperty("tabs", tabs());
    bar->setProperty("activeKey", QStringLiteral("home"));
    bar->setProperty("layout", QStringLiteral("bottom"));
    bar->setProperty("backgroundColor", QColor(0, 0, 0));
    QTest::qWait(50);
    const QPoint second(300, 30);  // the middle tab, inactive
    const QRgb before = window_->grabWindow().pixel(second.x() - 40, 8);
    QTest::mousePress(window_.get(), Qt::LeftButton, Qt::NoModifier, second);
    QTest::qWait(250);  // the plate's color animation
    const QRgb pressed = window_->grabWindow().pixel(second.x() - 40, 8);
    QVERIFY2(pressed != before, "a pressed tab is drawn differently from a resting one");
    QTest::mouseRelease(window_.get(), Qt::LeftButton, Qt::NoModifier, second);

    // Keyboard focus: Escape leaves the bar.
    bar->forceActiveFocus();
    QTest::keyClick(window_.get(), Qt::Key_Right);
    QVERIFY(bar->property("focusVisible").toBool());
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QVERIFY(!bar->property("focusVisible").toBool());
    QVERIFY(!bar->hasActiveFocus());
  }

  void tabBarMouseAndKeyboard() {
    auto bar = load(QStringLiteral("QtTabBar.qml"), QSize(600, 60));
    QVERIFY(bar);
    Recorder recorder;
    bar->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    bar->setProperty("tabs", tabs());
    bar->setProperty("activeKey", QStringLiteral("home"));
    bar->setProperty("layout", QStringLiteral("bottom"));
    QVERIFY(QTest::qWaitFor([&] { return bar->property("ready").toBool(); }));

    // Three tabs across 600 px: the second one spans x 200..400.
    const QPoint second = bar->mapToScene(QPointF(300, 30)).toPoint();
    QTest::mouseClick(window_.get(), Qt::LeftButton, Qt::NoModifier, second);
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("tabPressed:search")) == 1; }));

    bar->forceActiveFocus();
    QTest::keyClick(window_.get(), Qt::Key_Right);
    QVERIFY(bar->property("focusVisible").toBool());
    QTest::keyClick(window_.get(), Qt::Key_Return);
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("tabPressed:inbox")) == 1; }));
    QTest::keyClick(window_.get(), Qt::Key_Space);
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("tabPressed:inbox")) == 2; }));
    QTest::keyClick(window_.get(), Qt::Key_Home);
    QTest::keyClick(window_.get(), Qt::Key_Return);
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("tabPressed:home")) == 1; }));
  }

  void tabBarLongPress() {
    auto bar = load(QStringLiteral("QtTabBar.qml"), QSize(600, 60));
    QVERIFY(bar);
    Recorder recorder;
    bar->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    bar->setProperty("tabs", tabs());
    bar->setProperty("layout", QStringLiteral("bottom"));
    QVERIFY(QTest::qWaitFor([&] { return bar->property("ready").toBool(); }));
    const QPoint first = bar->mapToScene(QPointF(100, 30)).toPoint();
    QTest::mousePress(window_.get(), Qt::LeftButton, Qt::NoModifier, first);
    QTest::qWait(900);
    QTest::mouseRelease(window_.get(), Qt::LeftButton, Qt::NoModifier, first);
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("tabLongPressed:home")) == 1; }));
    QCOMPARE(recorder.count(QStringLiteral("tabPressed")), 0);
  }

  // ---- ExpoImage view ---------------------------------------------------
  // Loading, painting and events are covered by ExpoImageItemTest; this checks
  // the QML wrapper that maps the mounting layer's accessibility properties
  // onto Qt's attached Accessible properties.

  void expoImageAccessibilityFollowsProps() {
    auto view = load(QStringLiteral("QtExpoImage.qml"), QSize(100, 100));
    QVERIFY(view);
    const auto read = [&](const char* path) { return QQmlProperty(view.get(), QString::fromLatin1(path), qmlContext(view.get())).read(); };
    QCOMPARE(read("Accessible.ignored").toBool(), true);  // not accessible by default
    view->setProperty("accessible", true);
    view->setProperty("accessibilityLabel", QStringLiteral("Profile photo"));
    view->setProperty("accessibilityHint", QStringLiteral("Opens the profile"));
    view->setProperty("accessibilityRole", QStringLiteral("button"));
    view->setProperty("accessibilityStateSelected", true);
    QCOMPARE(read("Accessible.ignored").toBool(), false);
    QCOMPARE(read("Accessible.name").toString(), QStringLiteral("Profile photo"));
    QCOMPARE(read("Accessible.description").toString(), QStringLiteral("Opens the profile"));
    QCOMPARE(read("Accessible.role").toInt(), static_cast<int>(QAccessible::Button));
    QCOMPARE(read("Accessible.selected").toBool(), true);
    view->setProperty("accessibilityRole", QStringLiteral("image"));
    QCOMPARE(read("Accessible.role").toInt(), static_cast<int>(QAccessible::Graphic));
    view->setProperty("accessibilityStateDisabled", true);
    QCOMPARE(view->property("enabled").toBool(), false);
  }
};

}  // namespace

QTEST_MAIN(NativeUiQuickTest)
#include "NativeUiQuickTest.moc"
