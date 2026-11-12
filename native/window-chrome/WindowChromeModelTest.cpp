#include "WindowChromeModel.h"

#include <QJsonDocument>
#include <QtTest>

#include <stdexcept>

using namespace rnqnx::window;

namespace {

QJsonObject json(const char* text) {
  return QJsonDocument::fromJson(QByteArray(text)).object();
}

std::vector<WindowAction> actions(const ChromeLayout& layout) {
  std::vector<WindowAction> result;
  for (const auto& slot : layout.controls) result.push_back(slot.action);
  return result;
}

// Layout tests use a host that resizes its surface, so every control shows.
WindowCapabilities desktop() {
  return capabilitiesForPlatform(QStringLiteral("cocoa"), {}, SurfaceResize::FollowsWindow);
}

}  // namespace

class WindowChromeModelTest : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void parsesDefaults() {
    const auto config = parseWindowChromeConfig(QJsonObject{});
    QCOMPARE(config.controls, ControlsLayout::MacOS);
    QCOMPARE(config.titleBarStyle, TitleBarStyle::Default);
    QVERIFY(!config.initialSize.has_value());
  }

  void parsesResolvedDescriptor() {
    const auto config = parseWindowChromeConfig(json(
        R"({"controls":"windows","titleBarStyle":"hidden","initialSize":{"width":800,"height":600}})"));
    QCOMPARE(config.controls, ControlsLayout::Windows);
    QCOMPARE(config.titleBarStyle, TitleBarStyle::Hidden);
    QCOMPARE(config.initialSize.value(), QSize(800, 600));
  }

  void rejectsInvalidDescriptors_data() {
    QTest::addColumn<QString>("text");
    QTest::newRow("unknown key") << R"({"controls":"macos","frame":true})";
    QTest::newRow("unknown controls") << R"({"controls":"gnome"})";
    QTest::newRow("controls case") << R"({"controls":"MacOS"})";
    QTest::newRow("controls type") << R"({"controls":1})";
    QTest::newRow("unknown style") << R"({"titleBarStyle":"transparent"})";
    QTest::newRow("size not object") << R"({"initialSize":[800,600]})";
    QTest::newRow("size extra key") << R"({"initialSize":{"width":800,"height":600,"x":0}})";
    QTest::newRow("size missing height") << R"({"initialSize":{"width":800}})";
    QTest::newRow("size zero") << R"({"initialSize":{"width":0,"height":600}})";
    QTest::newRow("size fraction") << R"({"initialSize":{"width":800.5,"height":600}})";
    QTest::newRow("size too large") << R"({"initialSize":{"width":16385,"height":600}})";
    QTest::newRow("size string") << R"({"initialSize":{"width":"800","height":600}})";
  }

  void rejectsInvalidDescriptors() {
    QFETCH(QString, text);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             parseWindowChromeConfig(json(text.toUtf8().constData())));
  }

  void acceptsMaximumEdge() {
    const auto config =
        parseWindowChromeConfig(json(R"({"initialSize":{"width":16384,"height":1}})"));
    QCOMPARE(config.initialSize.value(), QSize(16384, 1));
  }

  void qnxMinimizesButCannotDrag() {
    const auto caps = capabilitiesForPlatform(QStringLiteral("qnx"));
    QVERIFY(caps.minimize.available);
    QVERIFY(caps.minimize.reason.isEmpty());
    QVERIFY(!caps.systemMove);
  }

  // fullscreen-winmgr keeps every window at the display size, so on QNX the
  // app frames its own window; maximize toggles that frame and needs an
  // initial size to come back to.
  void qnxFramesItsOwnWindow() {
    const auto caps = capabilitiesForPlatform(QStringLiteral("qnx"), {}, SurfaceResize::FollowsWindow);
    QVERIFY(caps.containedWindow);
    QVERIFY(caps.maximize.available);
    QVERIFY(!caps.systemMove);
    const auto fullScreen = layoutChrome({}, caps, 1920);
    QCOMPARE(actions(fullScreen), (std::vector<WindowAction>{WindowAction::Close, WindowAction::Minimize}));
    QCOMPARE(fullScreen.hidden.size(), size_t(1));
    QVERIFY(fullScreen.hidden.at(0).reason.contains(QStringLiteral("initialSize")));
    WindowChromeConfig windowed;
    windowed.initialSize = QSize(1280, 800);
    QCOMPARE(actions(layoutChrome(windowed, caps, 1280)),
             (std::vector<WindowAction>{WindowAction::Close, WindowAction::Minimize, WindowAction::Maximize}));
    QVERIFY(!desktop().containedWindow);
  }

  void resizeBandCoversEdgesAndCorners() {
    const QSizeF frame(400, 300);
    const std::vector<QRectF> controls{QRectF(4, 0, 44, 44)};
    QCOMPARE(resizeEdgesAt(QPointF(200, 150), frame, controls), Qt::Edges());
    QCOMPARE(resizeEdgesAt(QPointF(2, 150), frame, controls), Qt::Edges(Qt::LeftEdge));
    QCOMPARE(resizeEdgesAt(QPointF(-4, 150), frame, controls), Qt::Edges(Qt::LeftEdge));
    QCOMPARE(resizeEdgesAt(QPointF(399, 150), frame, controls), Qt::Edges(Qt::RightEdge));
    QCOMPARE(resizeEdgesAt(QPointF(200, 298), frame, controls), Qt::Edges(Qt::BottomEdge));
    QCOMPARE(resizeEdgesAt(QPointF(200, 1), frame, controls), Qt::Edges(Qt::TopEdge));
    QCOMPARE(resizeEdgesAt(QPointF(398, 290), frame, controls), Qt::RightEdge | Qt::BottomEdge);
    QCOMPARE(resizeEdgesAt(QPointF(390, 298), frame, controls), Qt::RightEdge | Qt::BottomEdge);
    QCOMPARE(resizeEdgesAt(QPointF(2, 2), frame, {}), Qt::LeftEdge | Qt::TopEdge);
    // A control in the top-left corner keeps its clicks.
    QCOMPARE(resizeEdgesAt(QPointF(10, 2), frame, controls), Qt::Edges());
    QCOMPARE(resizeEdgesAt(QPointF(-20, 150), frame, controls), Qt::Edges());
  }

  void resizeKeepsMinimumAndScreen() {
    const QRect start(100, 100, 800, 600);
    const QRect screen(0, 0, 1920, 1080);
    const QSize minimum(kMinWindowWidth, kMinWindowHeight);
    QCOMPARE(resizedGeometry(start, Qt::RightEdge | Qt::BottomEdge, QPoint(50, 30), minimum, screen),
             QRect(100, 100, 850, 630));
    QCOMPARE(resizedGeometry(start, Qt::LeftEdge, QPoint(-30, 0), minimum, screen), QRect(70, 100, 830, 600));
    // Never below the minimum; the right edge stays where it was.
    QCOMPARE(resizedGeometry(start, Qt::LeftEdge | Qt::TopEdge, QPoint(700, 700), minimum, screen),
             QRect(500, 400, 400, 300));
    // Never past the screen.
    QCOMPARE(resizedGeometry(start, Qt::RightEdge | Qt::BottomEdge, QPoint(5000, 5000), minimum, screen),
             QRect(100, 100, 1820, 980));
    QCOMPARE(resizedGeometry(start, Qt::LeftEdge | Qt::TopEdge, QPoint(-500, -500), minimum, screen),
             QRect(0, 0, 900, 700));
  }

  void moveKeepsTheBarReachable() {
    const QRect start(100, 100, 800, 600);
    const QRect screen(0, 0, 1920, 1080);
    QCOMPARE(movedGeometry(start, QPoint(20, -30), screen), QRect(120, 70, 800, 600));
    QCOMPARE(movedGeometry(start, QPoint(0, -500), screen).top(), 0);
    QCOMPARE(movedGeometry(start, QPoint(0, 5000), screen).top(), 1080 - 44);
    QCOMPARE(movedGeometry(start, QPoint(5000, 0), screen).left(), 1920 - kKeepVisible);
    QCOMPARE(movedGeometry(start, QPoint(-5000, 0), screen).left(), -800 + kKeepVisible);
  }

  // Review item 2: the host starts the React surface once, so maximize is
  // hidden everywhere until the surface follows the window.
  void fixedSurfaceHidesMaximize_data() {
    QTest::addColumn<QString>("platform");
    QTest::addColumn<QStringList>("options");
    QTest::newRow("qnx") << "qnx" << QStringList{};
    QTest::newRow("cocoa") << "cocoa" << QStringList{};
    QTest::newRow("xcb") << "xcb" << QStringList{};
    QTest::newRow("offscreen") << "offscreen" << QStringList{};
  }

  void fixedSurfaceHidesMaximize() {
    QFETCH(QString, platform);
    QFETCH(QStringList, options);
    const auto caps = capabilitiesForPlatform(platform, options);
    QVERIFY(!caps.maximize.available);
    QVERIFY(caps.maximize.reason.contains(QStringLiteral("first size")));
    QVERIFY(capabilitiesForPlatform(platform, options, SurfaceResize::FollowsWindow).maximize.available);
    const auto layout = layoutChrome({}, caps, 800);
    for (const auto& slot : layout.controls) QVERIFY(slot.action != WindowAction::Maximize);
  }

  void waylandIsCloseOnlyUntilVerified() {
    for (const auto surface : {SurfaceResize::Fixed, SurfaceResize::FollowsWindow}) {
      const auto caps = capabilitiesForPlatform(QStringLiteral("wayland"), {}, surface);
      QVERIFY(!caps.minimize.available);
      QVERIFY(!caps.maximize.available);
      QVERIFY(!caps.systemMove);
      QVERIFY(caps.minimize.reason.contains(QStringLiteral("Wayland")));
    }
    QVERIFY(!capabilitiesForPlatform(QStringLiteral("wayland-egl")).minimize.available);
  }

  void knowsWhenTheQpaForcesFullScreen() {
    QVERIFY(platformForcesFullScreen(QStringLiteral("qnx"), {}));
    QVERIFY(platformForcesFullScreen(QStringLiteral("qnx"), {QStringLiteral("desktop")}));
    QVERIFY(!platformForcesFullScreen(QStringLiteral("qnx"), {QStringLiteral("no-fullscreen")}));
    QVERIFY(!platformForcesFullScreen(QStringLiteral("cocoa"), {}));
    QVERIFY(!platformForcesFullScreen(QStringLiteral("offscreen"), {}));
  }

  void desktopPluginsKeepEveryControl_data() {
    QTest::addColumn<QString>("platform");
    for (const char* name : {"cocoa", "windows", "xcb"}) QTest::newRow(name) << name;
  }

  void desktopPluginsKeepEveryControl() {
    QFETCH(QString, platform);
    const auto caps = capabilitiesForPlatform(platform, {}, SurfaceResize::FollowsWindow);
    QVERIFY(caps.minimize.available);
    QVERIFY(caps.maximize.available);
    QVERIFY(caps.systemMove);
  }

  void unknownPluginGetsCloseOnly() {
    const auto caps = capabilitiesForPlatform(QStringLiteral("eglfs"));
    QVERIFY(!caps.minimize.available);
    QVERIFY(!caps.maximize.available);
    QVERIFY(!caps.systemMove);
    const auto layout = layoutChrome({}, caps, 640);
    QCOMPARE(actions(layout), std::vector<WindowAction>{WindowAction::Close});
    QCOMPARE(layout.hidden.size(), size_t(2));
  }

  void readsPlatformOptionsFromArguments() {
    const char* argv[] = {"app", "-platform", "qnx:no-fullscreen", "--bundle", "x"};
    const QStringList specs = platformSpecsFromArguments(5, argv, QString());
    QCOMPARE(platformOptionsFor(QStringLiteral("qnx"), specs),
             QStringList{QStringLiteral("no-fullscreen")});
    const char* equals[] = {"app", "--platform=qnx:desktop:no-fullscreen"};
    QCOMPARE(platformOptionsFor(QStringLiteral("qnx"), platformSpecsFromArguments(2, equals, QString())),
             (QStringList{QStringLiteral("desktop"), QStringLiteral("no-fullscreen")}));
    const char* none[] = {"app"};
    QCOMPARE(platformOptionsFor(QStringLiteral("qnx"),
                                platformSpecsFromArguments(1, none, QStringLiteral("qnx:no-fullscreen"))),
             QStringList{QStringLiteral("no-fullscreen")});
    QVERIFY(platformOptionsFor(QStringLiteral("qnx"),
                               platformSpecsFromArguments(1, none, QStringLiteral("qnx")))
                .isEmpty());
    // The argument wins over the environment, as in QGuiApplication.
    QCOMPARE(platformSpecsFromArguments(3, argv, QStringLiteral("cocoa:x")),
             QStringList{QStringLiteral("qnx:no-fullscreen")});
    const char* dangling[] = {"app", "-platform"};
    QVERIFY(platformSpecsFromArguments(2, dangling, QString()).isEmpty());
  }

  // Review item 8: QT_QPA_PLATFORM may hold a `;` fallback list. Options come
  // from the entry for the QPA that actually loaded.
  void readsOptionsFromFallbackLists() {
    const char* none[] = {"app"};
    const QStringList specs =
        platformSpecsFromArguments(1, none, QStringLiteral("wayland:foo;qnx:no-fullscreen;xcb"));
    QCOMPARE(specs.size(), 3);
    QCOMPARE(platformOptionsFor(QStringLiteral("qnx"), specs),
             QStringList{QStringLiteral("no-fullscreen")});
    QCOMPARE(platformOptionsFor(QStringLiteral("wayland"), specs), QStringList{QStringLiteral("foo")});
    QVERIFY(platformOptionsFor(QStringLiteral("xcb"), specs).isEmpty());
    QVERIFY(platformOptionsFor(QStringLiteral("cocoa"), specs).isEmpty());
  }

  void windowedSpecAddsNoFullscreenToQnxOnly() {
    QCOMPARE(windowedPlatformSpec(QString(), true), QStringLiteral("qnx:no-fullscreen"));
    QCOMPARE(windowedPlatformSpec(QString(), false), QString());
    QCOMPARE(windowedPlatformSpec(QStringLiteral("qnx"), true), QStringLiteral("qnx:no-fullscreen"));
    QCOMPARE(windowedPlatformSpec(QStringLiteral("qnx:no-fullscreen"), true),
             QStringLiteral("qnx:no-fullscreen"));
    QCOMPARE(windowedPlatformSpec(QStringLiteral("qnx:flush-screen-context"), true),
             QStringLiteral("qnx:flush-screen-context:no-fullscreen"));
    QCOMPARE(windowedPlatformSpec(QStringLiteral("offscreen;qnx"), true),
             QStringLiteral("offscreen;qnx:no-fullscreen"));
    QCOMPARE(windowedPlatformSpec(QStringLiteral("cocoa"), false), QStringLiteral("cocoa"));
  }

  void macosLayoutPutsControlsLeftInOrder() {
    const auto layout = layoutChrome({}, desktop(), 800);
    QVERIFY(layout.barVisible);
    QCOMPARE(layout.barHeight, kBarHeight);
    QCOMPARE(layout.contentInset, kBarHeight);
    QCOMPARE(actions(layout), (std::vector<WindowAction>{WindowAction::Close, WindowAction::Minimize,
                                                         WindowAction::Maximize}));
    QCOMPARE(layout.controls.front().hitRect.left(), kEdgePadding);
    for (size_t i = 1; i < layout.controls.size(); ++i)
      QCOMPARE(layout.controls[i].hitRect.left(), layout.controls[i - 1].hitRect.right());
    QCOMPARE(layout.titleAlignment, Qt::AlignHCenter);
    // Title is centred on the window, clear of the controls on both sides.
    QCOMPARE(layout.titleRect.center().x(), 400.0);
    QVERIFY(layout.titleRect.left() >= layout.controls.back().hitRect.right());
  }

  void windowsLayoutPutsControlsRightWithCloseAtEdge() {
    WindowChromeConfig config;
    config.controls = ControlsLayout::Windows;
    const auto layout = layoutChrome(config, desktop(), 800);
    QCOMPARE(actions(layout), (std::vector<WindowAction>{WindowAction::Minimize,
                                                         WindowAction::Maximize, WindowAction::Close}));
    QCOMPARE(layout.controls.back().hitRect.right(), 800.0);
    QCOMPARE(layout.controls.front().hitRect.width(), kWindowsHitWidth);
    QCOMPARE(layout.titleAlignment, Qt::AlignLeft);
    QCOMPARE(layout.titleRect.left(), kTitlePadding);
    QVERIFY(layout.titleRect.right() <= layout.controls.front().hitRect.left());
  }

  void hitTargetsAreFingerSized() {
    for (const auto controls : {ControlsLayout::MacOS, ControlsLayout::Windows}) {
      WindowChromeConfig config;
      config.controls = controls;
      for (const auto& slot : layoutChrome(config, desktop(), 800).controls) {
        QVERIFY(slot.hitRect.width() >= 44);
        QVERIFY(slot.hitRect.height() >= 44);
        QVERIFY(!slot.accessibleName.isEmpty());
      }
    }
  }

  // A full-screen QNX app (no initialSize): maximize is the only control that
  // cannot run, and the remaining two close up.
  void hiddenControlsCloseUpOnQnx() {
    for (const auto controls : {ControlsLayout::MacOS, ControlsLayout::Windows}) {
      WindowChromeConfig config;
      config.controls = controls;
      const auto layout = layoutChrome(config, capabilitiesForPlatform(QStringLiteral("qnx")), 1920);
      const auto expected = controls == ControlsLayout::MacOS
                                ? std::vector<WindowAction>{WindowAction::Close, WindowAction::Minimize}
                                : std::vector<WindowAction>{WindowAction::Minimize, WindowAction::Close};
      QCOMPARE(actions(layout), expected);
      QCOMPARE(layout.hidden.size(), size_t(1));
      QCOMPARE(layout.hidden.front().action, WindowAction::Maximize);
      QVERIFY(!layout.hidden.front().reason.isEmpty());
      if (controls == ControlsLayout::MacOS)
        QCOMPARE(layout.controls.front().hitRect.left(), kEdgePadding);
      else
        QCOMPARE(layout.controls.back().hitRect.right(), 1920.0);
    }
  }

  void windowedQnxKeepsAllThreeLive() {
    const auto caps = capabilitiesForPlatform(QStringLiteral("qnx"), {}, SurfaceResize::FollowsWindow);
    WindowChromeConfig config;
    config.initialSize = QSize(1280, 800);
    const auto layout = layoutChrome(config, caps, 1280);
    QCOMPARE(actions(layout), (std::vector<WindowAction>{WindowAction::Close, WindowAction::Minimize,
                                                         WindowAction::Maximize}));
    QVERIFY(layout.hidden.empty());
    QVERIFY(layout.disabled.empty());
    for (const auto& slot : layout.controls) QVERIFY(slot.enabled);
  }

  void disabledModeKeepsAllThreeOnQnx() {
    WindowChromeConfig config;
    config.unavailableControls = UnavailableControls::Disable;
    const auto layout = layoutChrome(config, capabilitiesForPlatform(QStringLiteral("qnx")), 1920);
    QCOMPARE(actions(layout), (std::vector<WindowAction>{WindowAction::Close, WindowAction::Minimize,
                                                         WindowAction::Maximize}));
    QVERIFY(layout.hidden.empty());
    QCOMPARE(layout.disabled.size(), size_t(1));
    QVERIFY(layout.controls.at(0).enabled);
    QVERIFY(layout.controls.at(1).enabled);
    QVERIFY(!layout.controls.at(2).enabled);
    QVERIFY(!layout.controls.at(2).reason.isEmpty());
    // Live platforms are unchanged by the mode.
    QVERIFY(layoutChrome(config, desktop(), 800).disabled.empty());
  }

  void parsesUnavailableControls() {
    QCOMPARE(parseUnavailableControls(QStringLiteral("hide")), UnavailableControls::Hide);
    QCOMPARE(parseUnavailableControls(QStringLiteral("disable")), UnavailableControls::Disable);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parseUnavailableControls(QStringLiteral("grey")));
  }

  void hiddenStyleFloatsControlsWithoutInset() {
    WindowChromeConfig config;
    config.titleBarStyle = TitleBarStyle::Hidden;
    const auto layout = layoutChrome(config, desktop(), 800);
    QVERIFY(!layout.barVisible);
    QCOMPARE(layout.contentInset, 0.0);
    QVERIFY(layout.titleRect.isEmpty());
    QCOMPARE(layout.controls.size(), size_t(3));
  }

  void noneStyleDrawsNothing() {
    WindowChromeConfig config;
    config.titleBarStyle = TitleBarStyle::None;
    const auto layout = layoutChrome(config, desktop(), 800);
    QVERIFY(!layout.barVisible);
    QCOMPARE(layout.barHeight, 0.0);
    QCOMPARE(layout.contentInset, 0.0);
    QVERIFY(layout.controls.empty());
  }

  void narrowWindowCollapsesTitleNotControls() {
    const auto layout = layoutChrome({}, desktop(), 100);
    QCOMPARE(layout.controls.size(), size_t(3));
    QVERIFY(layout.titleRect.width() >= 0);
    QVERIFY(layout.titleRect.isEmpty());
    WindowChromeConfig config;
    config.controls = ControlsLayout::Windows;
    QVERIFY(layoutChrome(config, desktop(), 100).titleRect.isEmpty());
  }

  void clampsInitialSize_data() {
    QTest::addColumn<QSize>("requested");
    QTest::addColumn<QSize>("available");
    QTest::addColumn<QSize>("expected");
    QTest::newRow("fits") << QSize(800, 600) << QSize(1920, 1040) << QSize(800, 600);
    QTest::newRow("too wide") << QSize(4000, 600) << QSize(1920, 1040) << QSize(1920, 600);
    QTest::newRow("too tall") << QSize(800, 2000) << QSize(1920, 1040) << QSize(800, 1040);
    QTest::newRow("both") << QSize(16384, 16384) << QSize(1280, 720) << QSize(1280, 720);
    QTest::newRow("no screen") << QSize(4000, 3000) << QSize() << QSize(4000, 3000);
  }

  void clampsInitialSize() {
    QFETCH(QSize, requested);
    QFETCH(QSize, available);
    QFETCH(QSize, expected);
    QCOMPARE(clampWindowSize(requested, available), expected);
  }

  void namesRoundTrip() {
    QCOMPARE(parseControlsLayout(nameOf(ControlsLayout::Windows)), ControlsLayout::Windows);
    QCOMPARE(parseTitleBarStyle(nameOf(TitleBarStyle::None)), TitleBarStyle::None);
    QCOMPARE(nameOf(WindowAction::Minimize), QStringLiteral("minimize"));
  }
};

QTEST_GUILESS_MAIN(WindowChromeModelTest)
#include "WindowChromeModelTest.moc"
