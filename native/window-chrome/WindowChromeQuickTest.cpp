// Renders the real chrome QML in an offscreen QQuickWindow and drives it with
// Qt pointer and key events. Host evidence only; no QNX Screen involved.

#include "WindowChrome.h"

#include <QAccessible>
#include <QImage>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QSignalSpy>
#include <QtTest>

#include <memory>

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

  void qnxDrawsOnlyLiveControls() {
    for (const auto controls : {ControlsLayout::MacOS, ControlsLayout::Windows}) {
      WindowChromeConfig config;
      config.controls = controls;
      Harness harness(config, capabilitiesForPlatform(QStringLiteral("qnx")), QSize(1920, 1080));
      QCOMPARE(controlNames(harness), QStringList{QStringLiteral("close")});
      QVERIFY(harness.chrome->alwaysShowGlyphs());
      const QStringList hidden = harness.chrome->describeHiddenControls();
      QCOMPARE(hidden.size(), 2);
      QVERIFY(hidden.at(0).startsWith(QStringLiteral("RNQNX_WINDOW_CHROME platform=qnx hidden=")));
      // A stale call for a hidden control is ignored.
      QSignalSpy spy(harness.chrome.get(), &WindowChrome::actionTriggered);
      harness.chrome->trigger(QStringLiteral("minimize"));
      QCOMPARE(spy.count(), 0);
    }
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
    // The last bar row in device pixels is the 1 px separator.
    const QColor separator = frame.pixelColor(x, qRound(44 * expectedDpr_) - 1);
    QCOMPARE(separator, QColor(QStringLiteral("#d0d0d0")));
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
