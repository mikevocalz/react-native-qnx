// Drives the drawer, tab bar and ExpoImage QML with Qt pointer and key events.
// A recorder QObject stands in for the Fabric bridge, so each test checks the
// calls the real bridge would turn into events for JavaScript.

#include <QDir>
#include <QImage>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

#include "react-native-qnx/views/QtCssColor.h"

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
  Q_INVOKABLE void tabPressed(const QString& key) { calls << QStringLiteral("tabPressed:") + key; }
  Q_INVOKABLE void tabLongPressed(const QString& key) {
    calls << QStringLiteral("tabLongPressed:") + key;
  }
  Q_INVOKABLE void layoutModeChanged(const QString& mode, double size) {
    calls << QStringLiteral("layoutMode:%1:%2").arg(mode).arg(size);
  }
  Q_INVOKABLE QString resolveImage(const QString& uri) { return uri; }
  Q_INVOKABLE void imageLoadStarted() { calls << QStringLiteral("loadStart"); }
  Q_INVOKABLE void imageProgress(double) {}
  Q_INVOKABLE void imageLoaded(const QString&, double w, double h, const QString& cache) {
    calls << QStringLiteral("load:%1x%2:%3").arg(w).arg(h).arg(cache);
  }
  Q_INVOKABLE void imageFailed(const QString&) { calls << QStringLiteral("error"); }
  Q_INVOKABLE void imageDisplayed() { calls << QStringLiteral("display"); }

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

  void expoImageLoadsDisplaysAndFails() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QImage image(32, 16, QImage::Format_ARGB32);
    image.fill(Qt::red);
    const QString path = directory.filePath(QStringLiteral("red.png"));
    QVERIFY(image.save(path));

    auto view = load(QStringLiteral("QtExpoImage.qml"), QSize(100, 100));
    QVERIFY(view);
    Recorder recorder;
    view->setProperty("bridge", QVariant::fromValue(static_cast<QObject*>(&recorder)));
    view->setProperty("transitionMs", 40.0);
    view->setProperty("sourceUrl", QUrl::fromLocalFile(path).toString());
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("display")) == 1; }, 3000));
    QVERIFY(recorder.calls.contains(QStringLiteral("load:32x16:none")));
    QVERIFY(recorder.calls.indexOf(QStringLiteral("loadStart")) < recorder.calls.indexOf(QStringLiteral("display")));

    view->setProperty("sourceUrl", QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing.png"))).toString());
    QVERIFY(QTest::qWaitFor([&] { return recorder.count(QStringLiteral("error")) == 1; }, 3000));
  }

  void expoImageContentFitMapsToFillMode() {
    auto view = load(QStringLiteral("QtExpoImage.qml"), QSize(100, 100));
    QVERIFY(view);
    const struct { const char* fit; int mode; } cases[] = {
        {"cover", 2}, {"contain", 1}, {"fill", 0}, {"none", 6}};  // Image.PreserveAspectCrop, ...Fit, Stretch, Pad
    for (const auto& c : cases) {
      view->setProperty("contentFit", QString::fromLatin1(c.fit));
      QCOMPARE(view->property("fillMode").toInt(), c.mode);
    }
  }

  void expoImageTintDegradesWithoutEffects() {
    auto view = load(QStringLiteral("QtExpoImage.qml"), QSize(100, 100));
    QVERIFY(view);
    view->setProperty("tintColor", QColor(Qt::blue));
    view->setProperty("hasTint", true);
    // Either the effect item exists, or the module is missing and the view
    // says so and still draws. Both are acceptable; a crash or a throw is not.
    const bool haveEffect = view->property("effectItem").value<QObject*>() != nullptr;
    const bool missing = view->property("effectsMissing").toBool();
    QVERIFY(haveEffect || missing);
    qInfo() << "QtQuick.Effects available:" << haveEffect;
  }
};

}  // namespace

QTEST_MAIN(NativeUiQuickTest)
#include "NativeUiQuickTest.moc"
