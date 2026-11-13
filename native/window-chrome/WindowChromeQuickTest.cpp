// Renders the real chrome QML in an offscreen QQuickWindow and drives it with
// Qt pointer and key events. Host evidence only; no QNX Screen involved.

#include "WindowChrome.h"
#include "WindowHost.h"

#include <QAccessible>
#include <QImage>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QSignalSpy>
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalSocket>
#include <QGuiApplication>
#include <QLocalServer>
#include <QThread>
#include <QStyleHints>

#include <atomic>
#include <cstring>
#include <vector>
#include <memory>

#if defined(Q_OS_UNIX)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

using namespace rnqnx::window;

namespace {

QQuickItem* findItem(QQuickItem* root, const QString& name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem* child : root->childItems())
    if (QQuickItem* found = findItem(child, name)) return found;
  return nullptr;
}

void collect(QQuickItem* root, QList<QQuickItem*>& out) {
  for (QQuickItem* child : root->childItems()) {
    out.append(child);
    collect(child, out);
  }
}

// A SingleInstance listening in its own thread, the way a separate process
// would: it answers while the test thread waits inside replace/restore. The
// thread ends when the instance is told to quit (honourQuit) or on stop().
class InstanceThread final : public QThread {
 public:
  InstanceThread(QString appId, BuildKind kind, bool honourQuit)
      : appId_(std::move(appId)), kind_(kind), honourQuit_(honourQuit) {}
  ~InstanceThread() override { stop(); }

  bool waitListening() {
    start();
    for (int i = 0; i < 200 && !listening.load(); ++i) QThread::msleep(10);
    return listening.load();
  }
  void stop() {
    if (isRunning()) {
      quit();
      wait(3000);
    }
  }

  std::atomic<bool> listening{false};
  std::atomic<bool> finishedListening{false};
  std::atomic<int> restores{0};
  std::atomic<int> quits{0};

 protected:
  void run() override {
    SingleInstance instance(appId_, kind_);
    connect(&instance, &SingleInstance::restoreRequested, &instance, [this]() { ++restores; },
            Qt::DirectConnection);
    connect(&instance, &SingleInstance::quitRequested, &instance, [this]() {
      ++quits;
      if (honourQuit_) QThread::currentThread()->quit();
    }, Qt::DirectConnection);
    listening = instance.listen();
    finishedListening = true;
    exec();
  }

 private:
  QString appId_;
  BuildKind kind_;
  bool honourQuit_;
};

QString uniqueAppId(const char* name) {
  return QStringLiteral("org.rnqnx.%1%2").arg(QLatin1String(name)).arg(QCoreApplication::applicationPid());
}

struct Harness {
  explicit Harness(WindowChromeConfig config,
                   WindowCapabilities caps = capabilitiesForPlatform(QStringLiteral("cocoa"), {}, SurfaceResize::FollowsWindow),
                   QSize size = QSize(800, 600)) {
    window = std::make_unique<QQuickWindow>();
    window->setTitle(QStringLiteral("My App"));
    window->setColor(Qt::white);
    window->resize(size);
    chrome = std::make_unique<WindowChrome>(*window, engine, config, std::move(caps));
    // Stand-in for the Fabric root: fills the content item.
    fabric = std::make_unique<QQuickItem>();
    fabric->setParentItem(&chrome->contentRoot());
    fabric->setSize(chrome->contentRoot().size());
    window->show();
    window->requestActivate();
  }
  ~Harness() {
    fabric.reset();
    chrome.reset();
    window.reset();
  }

  QQuickItem* control(const QString& action) const {
    return findItem(chrome->chromeItem(), QStringLiteral("rnqnxWindowControl_") + action);
  }
  QPoint centre(const QString& action) const {
    QQuickItem* item = control(action);
    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
  }

  QQmlEngine engine;
  std::unique_ptr<QQuickWindow> window;
  std::unique_ptr<WindowChrome> chrome;
  std::unique_ptr<QQuickItem> fabric;
};

QStringList controlNames(const Harness& harness) {
  QStringList names;
  QList<QQuickItem*> items;
  collect(harness.chrome->chromeItem(), items);
  for (auto* item : items) {
    if (item->objectName().startsWith(QStringLiteral("rnqnxWindowControl_")) && item->isVisible())
      names.append(item->objectName().mid(19));
  }
  names.sort();
  return names;
}

}  // namespace

class WindowChromeQuickTest : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void initTestCase() {
    const QByteArray expected = qgetenv("RNQNX_EXPECT_DPR");
    expectedDpr_ = expected.isEmpty() ? 1.0 : expected.toDouble();
  }

  void defaultStyleMountsFabricBelowBar() {
    Harness harness({});
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    QCOMPARE(harness.chrome->contentRoot().y(), 44.0);
    QCOMPARE(harness.chrome->contentRoot().height(), 556.0);
    QCOMPARE(harness.fabric->mapToScene(QPointF(0, 0)), QPointF(0, 44));
    auto* title = findItem(harness.chrome->chromeItem(), QStringLiteral("rnqnxWindowTitle"));
    QVERIFY(title);
    QCOMPARE(title->property("text").toString(), QStringLiteral("My App"));
    harness.window->setTitle(QStringLiteral("Renamed"));
    QCOMPARE(title->property("text").toString(), QStringLiteral("Renamed"));
  }

  void contentRootTracksWindowSize() {
    Harness harness({});
    harness.window->resize(1000, 700);
    QTRY_COMPARE(harness.chrome->contentRoot().width(), 1000.0);
    QCOMPARE(harness.chrome->contentRoot().height(), 656.0);
    QCOMPARE(harness.control(QStringLiteral("close"))->x(), kEdgePadding);
  }

  void layoutsPlaceControlsOnTheirSide() {
    Harness mac({});
    QVERIFY(mac.centre(QStringLiteral("close")).x() < mac.centre(QStringLiteral("maximize")).x());
    QVERIFY(mac.centre(QStringLiteral("maximize")).x() < 400);
    WindowChromeConfig config;
    config.controls = ControlsLayout::Windows;
    Harness windows(config);
    QVERIFY(windows.centre(QStringLiteral("minimize")).x() > 400);
    QVERIFY(windows.centre(QStringLiteral("close")).x() > windows.centre(QStringLiteral("maximize")).x());
  }

  // Full-screen QNX: close and minimize are live, maximize is left out.
  void qnxDrawsOnlyLiveControls() {
    for (const auto controls : {ControlsLayout::MacOS, ControlsLayout::Windows}) {
      WindowChromeConfig config;
      config.controls = controls;
      Harness harness(config, capabilitiesForPlatform(QStringLiteral("qnx")), QSize(1920, 1080));
      QCOMPARE(controlNames(harness), (QStringList{QStringLiteral("close"), QStringLiteral("minimize")}));
      QVERIFY(harness.chrome->alwaysShowGlyphs());
      const QStringList hidden = harness.chrome->describeHiddenControls();
      QCOMPARE(hidden.size(), 1);
      QVERIFY(hidden.at(0).startsWith(QStringLiteral("RNQNX_WINDOW_CHROME platform=qnx hidden=maximize")));
      // A stale call for a hidden control is ignored.
      QSignalSpy spy(harness.chrome.get(), &WindowChrome::actionTriggered);
      harness.chrome->trigger(QStringLiteral("maximize"));
      QCOMPARE(spy.count(), 0);
    }
  }

  void disabledControlsAreDrawnButInert() {
    WindowChromeConfig config;
    config.unavailableControls = UnavailableControls::Disable;
    Harness harness(config, capabilitiesForPlatform(QStringLiteral("qnx")), QSize(1920, 1080));
    QCOMPARE(controlNames(harness),
        (QStringList{QStringLiteral("close"), QStringLiteral("maximize"), QStringLiteral("minimize")}));
    QVERIFY(harness.chrome->describeHiddenControls().at(0).contains(QStringLiteral("disabled=maximize")));
    QSignalSpy triggered(harness.chrome.get(), &WindowChrome::actionTriggered);
    QSignalSpy unavailable(harness.chrome.get(), &WindowChrome::actionUnavailable);
    harness.chrome->trigger(QStringLiteral("maximize"));
    QCOMPARE(triggered.count(), 0);
    QCOMPARE(unavailable.count(), 1);
  }

  void hiddenStyleFloatsControlsOverContent() {
    WindowChromeConfig config;
    config.titleBarStyle = TitleBarStyle::Hidden;
    Harness harness(config);
    QCOMPARE(harness.chrome->contentRoot().y(), 0.0);
    auto* bar = findItem(harness.chrome->chromeItem(), QStringLiteral("rnqnxTitleBar"));
    QVERIFY(bar && !bar->isVisible());
    QCOMPARE(controlNames(harness).size(), 3);
  }

  void noneStyleDrawsNothing() {
    WindowChromeConfig config;
    config.titleBarStyle = TitleBarStyle::None;
    Harness harness(config);
    QVERIFY(!harness.chrome->chromeItem()->isVisible());
    QCOMPARE(harness.chrome->contentRoot().y(), 0.0);
    QCOMPARE(harness.chrome->contentRoot().height(), 600.0);
  }

  void controlsHaveAccessibleNamesAndRoles() {
    Harness harness({});
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    for (const auto& [action, name] :
         {std::pair{"close", "Close window"}, std::pair{"minimize", "Minimize window"},
          std::pair{"maximize", "Maximize window"}}) {
      QQuickItem* item = harness.control(QString::fromLatin1(action));
      QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(item);
      QVERIFY2(iface, action);
      QCOMPARE(iface->role(), QAccessible::Button);
      QCOMPARE(iface->text(QAccessible::Name), QString::fromLatin1(name));
      QVERIFY(iface->state().focusable);
      QVERIFY(iface->actionInterface());
      QVERIFY(iface->actionInterface()->actionNames().contains(QAccessibleActionInterface::pressAction()));
    }
  }

  void accessiblePressActionTriggers() {
    Harness harness({});
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    QSignalSpy spy(harness.chrome.get(), &WindowChrome::actionTriggered);
    QAccessibleInterface* iface =
        QAccessible::queryAccessibleInterface(harness.control(QStringLiteral("maximize")));
    iface->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("maximize"));
  }

  void keyboardReachesEveryControlWithVisibleFocus() {
    Harness harness({});
    QVERIFY(QTest::qWaitForWindowActive(harness.window.get()));
    QStringList order;
    for (int i = 0; i < 3; ++i) {
      QTest::keyClick(harness.window.get(), Qt::Key_Tab);
      QQuickItem* focused = harness.window->activeFocusItem();
      QVERIFY(focused);
      order.append(focused->objectName().mid(19));
      auto* ring = findItem(focused, QStringLiteral("focusRing"));
      QVERIFY(ring && ring->isVisible());
    }
    QCOMPARE(order, (QStringList{QStringLiteral("close"), QStringLiteral("minimize"),
                                 QStringLiteral("maximize")}));
    QSignalSpy spy(harness.chrome.get(), &WindowChrome::actionTriggered);
    QTest::keyClick(harness.window.get(), Qt::Key_Space);
    QTest::keyClick(harness.window.get(), Qt::Key_Return);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("maximize"));
  }

  void keyboardCloseClosesWindow() {
    WindowChromeConfig config;
    config.controls = ControlsLayout::Windows;
    Harness harness(config);
    QVERIFY(QTest::qWaitForWindowActive(harness.window.get()));
    // Windows order: minimize, maximize, close.
    for (int i = 0; i < 3; ++i) QTest::keyClick(harness.window.get(), Qt::Key_Tab);
    QCOMPARE(harness.window->activeFocusItem()->objectName(),
             QStringLiteral("rnqnxWindowControl_close"));
    QTest::keyClick(harness.window.get(), Qt::Key_Return);
    QTRY_VERIFY(!harness.window->isVisible());
  }

  // Review item 1: a resize must not recreate the controls, or keyboard focus
  // falls off the focused control.
  void keyboardFocusSurvivesResize() {
    for (const auto controls : {ControlsLayout::Windows, ControlsLayout::MacOS}) {
      WindowChromeConfig config;
      config.controls = controls;
      Harness harness(config);
      QVERIFY(QTest::qWaitForWindowActive(harness.window.get()));
      // QPointer: a recreated delegate shows up as a null pointer, not a crash.
      QPointer<QQuickItem> maximize = harness.control(QStringLiteral("maximize"));
      maximize->forceActiveFocus(Qt::TabFocusReason);
      QCOMPARE(harness.window->activeFocusItem(), maximize);
      const qreal before = maximize->x();
      harness.window->resize(1100, 700);
      QTRY_COMPARE(harness.chrome->contentRoot().width(), 1100.0);
      QVERIFY2(maximize, "the maximize control was destroyed by the resize");
      QCOMPARE(harness.control(QStringLiteral("maximize")), maximize.data());
      QCOMPARE(harness.window->activeFocusItem(), maximize.data());
      QCOMPARE(harness.window->activeFocusItem()->objectName(),
               QStringLiteral("rnqnxWindowControl_maximize"));
      if (controls == ControlsLayout::Windows)
        QCOMPARE(maximize->x(), before + 300);  // Right-anchored row moved with the edge.
      else
        QCOMPARE(maximize->x(), before);
      QVERIFY(findItem(maximize, QStringLiteral("focusRing"))->isVisible());
    }
  }

  // Review item 2: with a surface that keeps its first size, maximize is not
  // drawn and the reason is logged.
  void fixedSurfaceDrawsNoMaximize() {
    Harness harness({}, capabilitiesForPlatform(QStringLiteral("cocoa")));
    QCOMPARE(controlNames(harness), (QStringList{QStringLiteral("close"), QStringLiteral("minimize")}));
    const QStringList hidden = harness.chrome->describeHiddenControls();
    QCOMPARE(hidden.size(), 1);
    QVERIFY(hidden.at(0).contains(QStringLiteral("hidden=maximize reason=the React surface keeps its first size")));
  }

  void pointerTogglesMaximizeWithoutTakingFocus() {
    Harness harness({}, capabilitiesForPlatform(QStringLiteral("offscreen"), {}, SurfaceResize::FollowsWindow), QSize(640, 480));
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    QSignalSpy spy(harness.chrome.get(), &WindowChrome::actionTriggered);
    const QPoint point = harness.centre(QStringLiteral("maximize"));
    QTest::mouseMove(harness.window.get(), point);
    QTRY_VERIFY(harness.control(QStringLiteral("maximize"))->property("hovered").toBool());
    QTest::mouseClick(harness.window.get(), Qt::LeftButton, {}, point);
    QTRY_COMPARE(spy.count(), 1);
    QTRY_VERIFY(harness.window->windowStates() & Qt::WindowMaximized);
    QVERIFY(harness.chrome->maximized());
    QAccessibleInterface* iface =
        QAccessible::queryAccessibleInterface(harness.control(QStringLiteral("maximize")));
    QCOMPARE(iface->text(QAccessible::Name), QStringLiteral("Restore window"));
    auto* ring = findItem(harness.control(QStringLiteral("maximize")), QStringLiteral("focusRing"));
    QVERIFY(!ring->isVisible());
    harness.chrome->trigger(QStringLiteral("maximize"));
    QTRY_VERIFY(!(harness.window->windowStates() & Qt::WindowMaximized));
  }

  // The green light's glyph follows the window, not the last press: expand
  // while compact, restore while the window fills the screen, including when
  // a window manager sized it to the screen without a state change.
  void expandGlyphFollowsWindowState_data() {
    QTest::addColumn<int>("controls");
    QTest::addColumn<QString>("glyphItem");
    QTest::newRow("macos") << static_cast<int>(ControlsLayout::MacOS) << QStringLiteral("rnqnxExpandGlyph");
    QTest::newRow("windows") << static_cast<int>(ControlsLayout::Windows) << QStringLiteral("rnqnxMaximizeGlyph");
  }
  void expandGlyphFollowsWindowState() {
    QFETCH(int, controls);
    QFETCH(QString, glyphItem);
    WindowChromeConfig config;
    config.controls = static_cast<ControlsLayout>(controls);
    Harness harness(config, capabilitiesForPlatform(QStringLiteral("offscreen"), {}, SurfaceResize::FollowsWindow), QSize(480, 320));
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    QQuickItem* glyph = findItem(harness.control(QStringLiteral("maximize")), glyphItem);
    QVERIFY(glyph);
    QVERIFY(glyph->isVisible());
    QCOMPARE(glyph->property("glyph").toString(), QStringLiteral("expand"));
    QVERIFY(!harness.chrome->maximized());

    harness.chrome->trigger(QStringLiteral("maximize"));
    QTRY_VERIFY(harness.window->windowStates() & Qt::WindowMaximized);
    QTRY_COMPARE(glyph->property("glyph").toString(), QStringLiteral("restore"));
    harness.chrome->trigger(QStringLiteral("maximize"));
    QTRY_VERIFY(!(harness.window->windowStates() & Qt::WindowMaximized));
    QTRY_COMPARE(glyph->property("glyph").toString(), QStringLiteral("expand"));

    // A window manager sizing the window to the screen, state unchanged.
    const QRect screen = harness.window->screen()->availableGeometry();
    harness.window->setGeometry(screen);
    QTRY_COMPARE(harness.window->size(), screen.size());
    QVERIFY(!(harness.window->windowStates() & Qt::WindowMaximized));
    QTRY_COMPARE(glyph->property("glyph").toString(), QStringLiteral("restore"));
    QAccessibleInterface* iface =
        QAccessible::queryAccessibleInterface(harness.control(QStringLiteral("maximize")));
    QCOMPARE(iface->text(QAccessible::Name), QStringLiteral("Restore window"));
    harness.window->resize(480, 320);
    QTRY_COMPARE(glyph->property("glyph").toString(), QStringLiteral("expand"));
  }

  // QNX: the native window is display-sized and the chrome draws the app's
  // window inside it. It opens at initialSize, centred; the bar moves it,
  // edges and corners resize it, and maximize fills the display and back.
  void containedWindowMovesResizesAndExpands() {
    WindowChromeConfig config;
    config.initialSize = QSize(640, 400);
    Harness harness(config, capabilitiesForPlatform(QStringLiteral("qnx"), {}, SurfaceResize::FollowsWindow),
                    QSize(1600, 1000));
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    QVERIFY(harness.chrome->contained());
    QCOMPARE(harness.chrome->frameRect(), QRectF(480, 300, 640, 400));
    QCOMPARE(harness.chrome->chromeItem()->position(), QPointF(480, 300));
    QCOMPARE(harness.chrome->contentRoot().position(), QPointF(480, 344));
    QCOMPARE(harness.chrome->contentRoot().size(), QSizeF(640, 356));
    QCOMPARE(controlNames(harness),
             (QStringList{QStringLiteral("close"), QStringLiteral("maximize"), QStringLiteral("minimize")}));
    QQuickItem* glyph = findItem(harness.control(QStringLiteral("maximize")), QStringLiteral("rnqnxExpandGlyph"));
    QCOMPARE(glyph->property("glyph").toString(), QStringLiteral("expand"));
    auto* frame = findItem(harness.chrome->chromeItem(), QStringLiteral("rnqnxWindowFrame"));
    QVERIFY(frame && frame->isVisible());
    QSignalSpy pressed(harness.chrome.get(), &WindowChrome::actionTriggered);

    // Drag the bar from its middle: the frame follows the pointer.
    QPoint at(800, 320);
    QTest::mousePress(harness.window.get(), Qt::LeftButton, {}, at);
    for (int step = 1; step <= 5; ++step)
      QTest::mouseMove(harness.window.get(), at + QPoint(20 * step, 10 * step));
    QTest::mouseRelease(harness.window.get(), Qt::LeftButton, {}, at + QPoint(100, 50));
    QTRY_COMPARE(harness.chrome->frameRect(), QRectF(580, 350, 640, 400));
    QCOMPARE(harness.chrome->contentRoot().position(), QPointF(580, 394));

    // Hover on the right edge shows the horizontal hint; drag the bottom
    // right corner to grow, then far up-left to hit the minimum size.
    QTest::mouseMove(harness.window.get(), QPoint(1218, 550));
    QTRY_COMPARE(harness.chrome->resizeHint(), QStringLiteral("horizontal"));
    QVERIFY(findItem(harness.chrome->chromeItem(), QStringLiteral("rnqnxResizeHint"))->isVisible());
    at = QPoint(1218, 748);
    QTest::mouseMove(harness.window.get(), at);
    QTRY_COMPARE(harness.chrome->resizeHint(), QStringLiteral("diagonal"));
    QTest::mousePress(harness.window.get(), Qt::LeftButton, {}, at);
    QTest::mouseMove(harness.window.get(), at + QPoint(60, 40));
    QTest::mouseRelease(harness.window.get(), Qt::LeftButton, {}, at + QPoint(60, 40));
    QTRY_COMPARE(harness.chrome->frameRect(), QRectF(580, 350, 700, 440));
    QCOMPARE(harness.chrome->contentRoot().size(), QSizeF(700, 396));
    at = QPoint(1278, 788);
    QTest::mousePress(harness.window.get(), Qt::LeftButton, {}, at);
    QTest::mouseMove(harness.window.get(), at - QPoint(900, 900));
    QTest::mouseRelease(harness.window.get(), Qt::LeftButton, {}, at - QPoint(900, 900));
    QTRY_COMPARE(harness.chrome->frameRect(), QRectF(580, 350, kMinWindowWidth, kMinWindowHeight));
    QCOMPARE(pressed.count(), 0);  // No drag pressed a control.

    // Maximize fills the window; the outline goes and the glyph turns.
    harness.chrome->trigger(QStringLiteral("maximize"));
    QTRY_COMPARE(harness.chrome->frameRect(), QRectF(0, 0, 1600, 1000));
    QVERIFY(harness.chrome->maximized());
    QTRY_COMPARE(glyph->property("glyph").toString(), QStringLiteral("restore"));
    QVERIFY(!frame->isVisible());
    QCOMPARE(harness.chrome->contentRoot().size(), QSizeF(1600, 956));
    // No moving or resizing while it fills the display.
    QTest::mousePress(harness.window.get(), Qt::LeftButton, {}, QPoint(800, 20));
    QTest::mouseMove(harness.window.get(), QPoint(900, 120));
    QTest::mouseRelease(harness.window.get(), Qt::LeftButton, {}, QPoint(900, 120));
    QCOMPARE(harness.chrome->frameRect(), QRectF(0, 0, 1600, 1000));
    // Back to the size and place it had.
    harness.chrome->trigger(QStringLiteral("maximize"));
    QTRY_COMPARE(harness.chrome->frameRect(), QRectF(580, 350, kMinWindowWidth, kMinWindowHeight));
    QTRY_COMPARE(glyph->property("glyph").toString(), QStringLiteral("expand"));
    // A double click on the bar toggles too.
    const QPoint bar(580 + kMinWindowWidth / 2, 350 + 20);
    QTest::mouseDClick(harness.window.get(), Qt::LeftButton, {}, bar);
    QTRY_VERIFY(harness.chrome->maximized());
  }

  // QNX with rnqnx-winmgr: the native window is the app's window. The bar
  // moves it on its screen, edges resize it, minimize goes to the manager
  // and maximize uses Qt's window states.
  void sizeKeepingManagerMovesTheNativeWindow() {
    WindowChromeConfig config;
    config.initialSize = QSize(640, 400);
    Harness harness(config,
                    withSizeKeepingWindowManager(
                        capabilitiesForPlatform(QStringLiteral("qnx"), {}, SurfaceResize::FollowsWindow)),
                    QSize(640, 400));
    harness.window->setPosition(300, 200);
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    QVERIFY(!harness.chrome->contained());
    QVERIFY(harness.chrome->movesWindow());
    QCOMPARE(harness.chrome->frameRect(), QRectF(0, 0, 640, 400));
    QCOMPARE(harness.chrome->contentRoot().size(), QSizeF(640, 356));
    QCOMPARE(controlNames(harness),
             (QStringList{QStringLiteral("close"), QStringLiteral("maximize"), QStringLiteral("minimize")}));
    QVERIFY(!findItem(harness.chrome->chromeItem(), QStringLiteral("rnqnxWindowFrame"))->isVisible());
    QSignalSpy pressed(harness.chrome.get(), &WindowChrome::actionTriggered);

    // Drag the bar: the window follows the pointer on the screen. The
    // pointer moves in screen coordinates while the window moves under it.
    QWindow* window = harness.window.get();
    const auto pointerAt = [window](QPoint global) { return window->mapFromGlobal(global); };
    QPoint start = window->mapToGlobal(QPoint(320, 20));
    QTest::mousePress(window, Qt::LeftButton, {}, pointerAt(start));
    for (int step = 1; step <= 5; ++step)
      QTest::mouseMove(window, pointerAt(start + QPoint(20 * step, 10 * step)));
    QTest::mouseRelease(window, Qt::LeftButton, {}, pointerAt(start + QPoint(100, 50)));
    QTRY_COMPARE(harness.window->geometry(), QRect(400, 250, 640, 400));
    QCOMPARE(harness.chrome->frameRect(), QRectF(0, 0, 640, 400));

    // Drag the bottom right corner to grow the window.
    const QPoint at(638, 398);
    QTest::mouseMove(harness.window.get(), at);
    QTRY_COMPARE(harness.chrome->resizeHint(), QStringLiteral("diagonal"));
    QVERIFY(findItem(harness.chrome->chromeItem(), QStringLiteral("rnqnxResizeHint"))->isVisible());
    start = window->mapToGlobal(at);
    QTest::mousePress(window, Qt::LeftButton, {}, at);
    QTest::mouseMove(window, pointerAt(start + QPoint(60, 40)));
    QTest::mouseRelease(window, Qt::LeftButton, {}, pointerAt(start + QPoint(60, 40)));
    QTRY_COMPARE(harness.window->geometry(), QRect(400, 250, 700, 440));
    QTRY_COMPARE(harness.chrome->contentRoot().size(), QSizeF(700, 396));
    QCOMPARE(pressed.count(), 0);

    // Maximize fills the screen and comes back to the same place.
    harness.chrome->trigger(QStringLiteral("maximize"));
    QTRY_VERIFY(harness.chrome->maximized());
    // No moving while maximized.
    const QRect maximized = harness.window->geometry();
    QTest::mousePress(harness.window.get(), Qt::LeftButton, {}, QPoint(800, 20));
    QTest::mouseMove(harness.window.get(), QPoint(900, 120));
    QTest::mouseRelease(harness.window.get(), Qt::LeftButton, {}, QPoint(900, 120));
    QCOMPARE(harness.window->geometry(), maximized);
    harness.chrome->trigger(QStringLiteral("maximize"));
    QTRY_COMPARE(harness.window->geometry(), QRect(400, 250, 700, 440));

    // Minimize asks the manager; off QNX that request does nothing, and the
    // window is neither hidden nor destroyed here.
    harness.chrome->trigger(QStringLiteral("minimize"));
    QVERIFY(harness.window->isVisible());
    QVERIFY(harness.window->handle() != nullptr);
  }

  // A full-screen QNX app (no initialSize) is not contained and has no
  // maximize control.
  void qnxWithoutInitialSizeFillsTheWindow() {
    Harness harness({}, capabilitiesForPlatform(QStringLiteral("qnx"), {}, SurfaceResize::FollowsWindow), QSize(1600, 1000));
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    QVERIFY(!harness.chrome->contained());
    QCOMPARE(harness.chrome->frameRect(), QRectF(0, 0, 1600, 1000));
    QCOMPARE(controlNames(harness), (QStringList{QStringLiteral("close"), QStringLiteral("minimize")}));
  }

  void pointerMissOutsideTargetDoesNothing() {
    Harness harness({});
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    QSignalSpy spy(harness.chrome.get(), &WindowChrome::actionTriggered);
    QTest::mouseClick(harness.window.get(), Qt::LeftButton, {}, QPoint(400, 22));
    QTest::mouseClick(harness.window.get(), Qt::LeftButton, {}, QPoint(20, 100));
    QCOMPARE(spy.count(), 0);
  }

  void barScalesWithDevicePixelRatio() {
    Harness harness({}, capabilitiesForPlatform(QStringLiteral("offscreen")), QSize(400, 300));
    QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
    QCOMPARE(harness.window->devicePixelRatio(), expectedDpr_);
    // Logical layout is independent of the ratio.
    QCOMPARE(harness.chrome->contentRoot().y(), 44.0);
    QCOMPARE(harness.control(QStringLiteral("close"))->width(), 44.0);
    const QImage frame = harness.window->grabWindow();
    QCOMPARE(frame.height(), qRound(300 * expectedDpr_));
    const int x = qRound(200 * expectedDpr_);
    const QColor inBar = frame.pixelColor(x, qRound(20 * expectedDpr_));
    const QColor belowBar = frame.pixelColor(x, qRound(50 * expectedDpr_));
    QVERIFY2(inBar != QColor(Qt::white), qPrintable(inBar.name()));
    QCOMPARE(belowBar, QColor(Qt::white));
    // The last bar row in device pixels is the 1 px separator. The bar is
    // light only when the platform reports a light colour scheme.
    const QColor separator = frame.pixelColor(x, qRound(44 * expectedDpr_) - 1);
    const bool light = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light;
    QCOMPARE(separator, QColor(light ? QStringLiteral("#d0d0d0") : QStringLiteral("#060c14")));
  }

  // A second launch of the same app id hands a restore request to the first
  // and reports that it should exit; a different app id is not affected.
  void singleInstanceForwardsRestore() {
    const QString appId = QStringLiteral("org.rnqnx.test%1").arg(QCoreApplication::applicationPid());
    SingleInstance first(appId);
    QVERIFY(!first.forwardedToRunningInstance());
    QVERIFY(first.listen());
    QSignalSpy restored(&first, &SingleInstance::restoreRequested);
    SingleInstance second(appId);
    QVERIFY(second.forwardedToRunningInstance());
    QTRY_COMPARE(restored.count(), 1);
    SingleInstance other(appId + QStringLiteral("x"));
    QVERIFY(!other.forwardedToRunningInstance());
  }

  // Dev, debug and preview ids reduce to the base id, so every build of one
  // app uses one socket; the name has no uid in it.
  void baseAppIdStripsBuildVariants() {
    QCOMPARE(baseAppId(QStringLiteral("org.rnqnx.showcase")), QStringLiteral("org.rnqnx.showcase"));
    QCOMPARE(baseAppId(QStringLiteral("org.rnqnx.showcase.dev")), QStringLiteral("org.rnqnx.showcase"));
    QCOMPARE(baseAppId(QStringLiteral("org.rnqnx.showcase.debug")), QStringLiteral("org.rnqnx.showcase"));
    QCOMPARE(baseAppId(QStringLiteral("org.rnqnx.showcase.preview")), QStringLiteral("org.rnqnx.showcase"));
    QCOMPARE(baseAppId(QStringLiteral("org.rnqnx.showcase.dev.debug")), QStringLiteral("org.rnqnx.showcase"));
    // A part that only starts with a suffix is not one, and a two-part id keeps its last part.
    QCOMPARE(baseAppId(QStringLiteral("org.rnqnx.devtools")), QStringLiteral("org.rnqnx.devtools"));
    QCOMPARE(baseAppId(QStringLiteral("org.dev")), QStringLiteral("org.dev"));
    QCOMPARE(SingleInstance::socketNameFor(QStringLiteral("org.rnqnx.showcase.dev")),
             SingleInstance::socketNameFor(QStringLiteral("org.rnqnx.showcase")));
    QCOMPARE(QFileInfo(SingleInstance::socketNameFor(QStringLiteral("org.rnqnx.showcase"))).fileName(),
             QStringLiteral("rnqnx-org.rnqnx.showcase"));
    // One directory for every launcher, whatever its TMPDIR.
    QCOMPARE(QFileInfo(SingleInstance::socketNameFor(QStringLiteral("org.rnqnx.showcase"))).path(),
             QStringLiteral("/tmp"));
    QVERIFY(SingleInstance::socketNameFor(QStringLiteral("org.rnqnx.a")) !=
            SingleInstance::socketNameFor(QStringLiteral("org.rnqnx.b")));
    QCOMPARE(buildVariantSuffixes(), (QStringList{QStringLiteral("dev"), QStringLiteral("debug"), QStringLiteral("preview")}));
  }

  // A dev launch finds the running release copy: restore reaches it and the
  // instance reports its pid and kind.
  void devLaunchReachesReleaseInstance() {
    const QString appId = uniqueAppId("share");
    InstanceThread release(appId, BuildKind::Release, true);
    QVERIFY(release.waitListening());
    SingleInstance dev(appId + QStringLiteral(".dev"), BuildKind::Dev);
    QVERIFY(dev.forwardedToRunningInstance());
    QTRY_COMPARE(release.restores.load(), 1);
    const auto info = dev.runningInstanceInfo();
    QVERIFY(info.has_value());
    QCOMPARE(info->pid, static_cast<qint64>(QCoreApplication::applicationPid()));
    QCOMPARE(info->kind, QStringLiteral("release"));
    // A second listener for the same app cannot take the socket from the live one.
    SingleInstance again(appId, BuildKind::Release);
    QVERIFY(!again.listen());
  }

  // --replace: the running instance gets `quit`, its process exits, and the
  // new launch can listen on the same key. No signal is sent.
  void replaceQuitsRunningInstance() {
    const QString appId = uniqueAppId("replace");
    InstanceThread release(appId, BuildKind::Release, true);
    QVERIFY(release.waitListening());
    SingleInstance dev(appId + QStringLiteral(".dev"), BuildKind::Dev);
    int signalled = 0;
    // The thread stands in for the old process.
    dev.setProcessProbe([&release](qint64) { return release.isRunning(); },
                        [&signalled](qint64) { ++signalled; });
    QCOMPARE(dev.replaceRunningInstance(3000, 500), ReplaceResult::Replaced);
    QCOMPARE(release.quits.load(), 1);
    QCOMPARE(signalled, 0);
    // The key is free again for the new launch.
    QVERIFY(dev.listen());
    QLocalSocket reach;
    reach.connectToServer(SingleInstance::socketNameFor(appId));
    QVERIFY(reach.waitForConnected(500));
  }

  // An instance that ignores `quit` gets SIGTERM (once) after the quit wait.
  void replaceEscalatesToTerm() {
    const QString appId = uniqueAppId("term");
    InstanceThread stuck(appId, BuildKind::Release, false);
    QVERIFY(stuck.waitListening());
    SingleInstance dev(appId + QStringLiteral(".dev"), BuildKind::Dev);
    std::atomic<bool> gone{false};
    std::vector<qint64> signalled;
    dev.setProcessProbe([&gone](qint64) { return !gone.load(); },
                        [&gone, &signalled](qint64 pid) { signalled.push_back(pid); gone = true; });
    QCOMPARE(dev.replaceRunningInstance(200, 500), ReplaceResult::Replaced);
    QCOMPARE(stuck.quits.load(), 1);
    QCOMPARE(signalled.size(), size_t(1));
    QCOMPARE(signalled.front(), static_cast<qint64>(QCoreApplication::applicationPid()));
  }

  // When neither quit nor SIGTERM ends it, replace fails and starts nothing.
  void replaceFailsWhenInstanceWillNotStop() {
    const QString appId = uniqueAppId("stuck");
    InstanceThread stuck(appId, BuildKind::Release, false);
    QVERIFY(stuck.waitListening());
    SingleInstance dev(appId + QStringLiteral(".dev"), BuildKind::Dev);
    int signalled = 0;
    dev.setProcessProbe([](qint64) { return true; }, [&signalled](qint64) { ++signalled; });
    QCOMPARE(dev.replaceRunningInstance(100, 100), ReplaceResult::Failed);
    QCOMPARE(signalled, 1);
  }

  void replaceWithNothingRunning() {
    SingleInstance only(uniqueAppId("alone"), BuildKind::Dev);
    QCOMPARE(only.replaceRunningInstance(100, 100), ReplaceResult::NoneRunning);
    QVERIFY(!only.runningInstanceInfo().has_value());
  }

#if defined(Q_OS_UNIX)
  // A socket file left by a crashed process (nothing listening) neither
  // counts as a running instance nor stops the next launch from listening.
  void staleSocketIsRecovered() {
    const QString appId = uniqueAppId("stale");
    const QString name = SingleInstance::socketNameFor(appId);
    QString path;
    {
      QLocalServer probe;
      QVERIFY(probe.listen(name));
      path = probe.fullServerName();
    }
    QVERIFY(!QFileInfo::exists(path));
    // bind() without listen() and close(): the file stays, nobody answers.
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    QVERIFY(fd >= 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const QByteArray local = QFile::encodeName(path);
    QVERIFY(local.size() < static_cast<int>(sizeof address.sun_path));
    std::memcpy(address.sun_path, local.constData(), static_cast<size_t>(local.size()));
    QCOMPARE(::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof address), 0);
    ::close(fd);
    QVERIFY(QFileInfo::exists(path));

    SingleInstance next(appId, BuildKind::Release);
    QVERIFY(!next.forwardedToRunningInstance());
    QCOMPARE(next.replaceRunningInstance(100, 100), ReplaceResult::NoneRunning);
    QVERIFY(next.listen());
    // It is live now: the socket accepts connections.
    QLocalSocket reach;
    reach.connectToServer(name);
    QVERIFY(reach.waitForConnected(500));
  }
#endif

  // The socket protocol, one command per line, spoken raw.
  void socketProtocol() {
    const QString appId = uniqueAppId("proto");
    InstanceThread running(appId, BuildKind::Dev, true);
    QVERIFY(running.waitListening());
    const QString path = SingleInstance::socketNameFor(appId);
    const auto ask = [&path](const QByteArray& command) {
      QLocalSocket socket;
      socket.connectToServer(path);
      if (!socket.waitForConnected(1000)) return QByteArray("no connection");
      socket.write(command + "\n");
      socket.waitForBytesWritten(1000);
      if (!socket.canReadLine()) socket.waitForReadyRead(2000);
      return socket.readLine().trimmed();
    };
    const QByteArray pid = QByteArray::number(static_cast<qlonglong>(QCoreApplication::applicationPid()));
    const QByteArray expected = "ok pid=" + pid + " kind=dev";
    QCOMPARE(ask("info"), expected);
    QCOMPARE(ask("restore"), expected);
    QCOMPARE(running.restores.load(), 1);
    QCOMPARE(running.quits.load(), 0);
    QCOMPARE(ask("bogus"), QByteArray("error unknown command"));
    QCOMPARE(running.restores.load(), 1);
    // Any login can reach it: world read and write on the socket file.
    const auto permissions = QFileInfo(path).permissions();
    QVERIFY(permissions.testFlag(QFileDevice::ReadOther) && permissions.testFlag(QFileDevice::WriteOther));
    QCOMPARE(ask("quit"), expected);
    QCOMPARE(running.quits.load(), 1);
  }

  // The socket directory is created when it is missing.
  void missingSocketDirectoryIsCreated() {
    // Short: a socket path is limited to about 100 bytes.
    const QString root = QStringLiteral("/tmp/rq%1").arg(QCoreApplication::applicationPid());
    const QString dir = root + QStringLiteral("/missing/run");
    QVERIFY(!QFileInfo::exists(dir));
    struct EnvGuard {
      ~EnvGuard() { qunsetenv("RNQNX_SOCKET_DIR"); }
    } guard;
    qputenv("RNQNX_SOCKET_DIR", QFile::encodeName(dir));
    {
      SingleInstance instance(QStringLiteral("org.rnqnx.sockdir"), BuildKind::Release);
      QVERIFY(instance.listen());
      QVERIFY(QFileInfo(dir).isDir());
      QLocalSocket reach;
      reach.connectToServer(instance.serverName());
      QVERIFY(reach.waitForConnected(500));
    }
    QDir(root).removeRecursively();
  }

  // A stale socket another login left and this one cannot remove (here: not a
  // removable file) does not stop the app from owning the key: it listens on
  // its per-user fallback and later launches find it there.
  void unremovableStaleSocketUsesFallback() {
    const QString appId = uniqueAppId("fallback");
    const QString path = SingleInstance::socketNameFor(appId);
    QVERIFY(QDir().mkdir(path));  // a directory squats on the primary path
    SingleInstance owner(appId, BuildKind::Release);
    QVERIFY(owner.listen());
    SingleInstance later(appId + QStringLiteral(".dev"), BuildKind::Dev);
    QVERIFY(!later.listen());  // the key is owned; no second instance
    QVERIFY(later.candidateSockets().size() >= 2);
    QLocalSocket reach;
    reach.connectToServer(later.candidateSockets().at(1));
    QVERIFY(reach.waitForConnected(500));
    reach.abort();
    QDir().rmdir(path);
  }

  // Launches that start at the same moment: exactly one becomes the instance.
  void simultaneousLaunchesYieldOneOwner() {
    const QString appId = uniqueAppId("race");
    std::vector<std::unique_ptr<InstanceThread>> launches;
    for (int i = 0; i < 4; ++i)
      launches.push_back(std::make_unique<InstanceThread>(appId, BuildKind::Release, true));
    for (auto& launch : launches) launch->start();
    for (int i = 0; i < 300; ++i) {
      bool settled = true;
      for (auto& launch : launches) settled = settled && launch->finishedListening.load();
      if (settled) break;
      QThread::msleep(10);
    }
    int owners = 0;
    for (auto& launch : launches) owners += launch->listening.load() ? 1 : 0;
    QCOMPARE(owners, 1);
  }

  // Writes reference frames when RNQNX_CHROME_FRAMES_DIR is set; used for the
  // PR's host screenshots. Always checks that each frame renders.
  void rendersReferenceFrames() {
    const QString dir = qEnvironmentVariable("RNQNX_CHROME_FRAMES_DIR");
    struct Case {
      const char* name;
      ControlsLayout controls;
      TitleBarStyle style;
      const char* platform;
    };
    for (const Case& item : {Case{"macos-default", ControlsLayout::MacOS, TitleBarStyle::Default, "cocoa"},
                             Case{"windows-default", ControlsLayout::Windows, TitleBarStyle::Default, "cocoa"},
                             Case{"macos-hidden", ControlsLayout::MacOS, TitleBarStyle::Hidden, "cocoa"},
                             Case{"qnx-macos", ControlsLayout::MacOS, TitleBarStyle::Default, "qnx"},
                             Case{"qnx-windows", ControlsLayout::Windows, TitleBarStyle::Default, "qnx"}}) {
      WindowChromeConfig config;
      config.controls = item.controls;
      config.titleBarStyle = item.style;
      Harness harness(config, capabilitiesForPlatform(QString::fromLatin1(item.platform), {}, SurfaceResize::FollowsWindow), QSize(480, 160));
      harness.window->setColor(QColor(QStringLiteral("#dbe7f5")));
      QVERIFY(QTest::qWaitForWindowExposed(harness.window.get()));
      if (item.style == TitleBarStyle::Default && item.controls == ControlsLayout::MacOS &&
          QString::fromLatin1(item.platform) == QLatin1String("cocoa")) {
        // Show the keyboard focus ring on the first control in one frame.
        QTest::keyClick(harness.window.get(), Qt::Key_Tab);
      }
      const QImage frame = harness.window->grabWindow();
      QVERIFY(!frame.isNull());
      if (!dir.isEmpty()) QVERIFY(frame.save(dir + QLatin1Char('/') + QLatin1String(item.name) + QStringLiteral(".png")));
    }
  }

 private:
  qreal expectedDpr_{1.0};
};

QTEST_MAIN(WindowChromeQuickTest)
#include "WindowChromeQuickTest.moc"
