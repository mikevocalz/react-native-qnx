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

WindowCapabilities desktop() { return capabilitiesForPlatform(QStringLiteral("cocoa")); }

}  // namespace

class WindowChromeModelTest : public QObject {
  Q_OBJECT

 private slots:
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

  void qnxHidesMinimizeAndDrag() {
    const auto caps = capabilitiesForPlatform(QStringLiteral("qnx"));
    QVERIFY(!caps.minimize.available);
    QVERIFY(caps.minimize.reason.contains(QStringLiteral("minimize()")));
    QVERIFY(!caps.systemMove);
  }

  void qnxMaximizeDependsOnFullscreenOption() {
    const auto fullscreen = capabilitiesForPlatform(QStringLiteral("qnx"));
    QVERIFY(!fullscreen.maximize.available);
    QVERIFY(fullscreen.maximize.reason.contains(QStringLiteral("no-fullscreen")));
    const auto windowed = capabilitiesForPlatform(
        QStringLiteral("qnx"), {QStringLiteral("desktop"), QStringLiteral("no-fullscreen")});
    QVERIFY(windowed.maximize.available);
    QVERIFY(windowed.maximize.reason.isEmpty());
    QVERIFY(!windowed.minimize.available);
  }

  void desktopPluginsKeepEveryControl_data() {
    QTest::addColumn<QString>("platform");
    for (const char* name : {"cocoa", "windows", "xcb", "wayland"}) QTest::newRow(name) << name;
  }

  void desktopPluginsKeepEveryControl() {
    QFETCH(QString, platform);
    const auto caps = capabilitiesForPlatform(platform);
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
    QCOMPARE(platformOptionsFromArguments(5, argv, QString()),
             QStringList{QStringLiteral("no-fullscreen")});
    const char* equals[] = {"app", "--platform=qnx:desktop:no-fullscreen"};
    QCOMPARE(platformOptionsFromArguments(2, equals, QString()),
             (QStringList{QStringLiteral("desktop"), QStringLiteral("no-fullscreen")}));
    const char* none[] = {"app"};
    QCOMPARE(platformOptionsFromArguments(1, none, QStringLiteral("qnx:no-fullscreen")),
             QStringList{QStringLiteral("no-fullscreen")});
    QVERIFY(platformOptionsFromArguments(1, none, QStringLiteral("qnx")).isEmpty());
    // The argument wins over the environment, as in QGuiApplication.
    QVERIFY(platformOptionsFromArguments(3, argv, QStringLiteral("cocoa:x")).contains(
        QStringLiteral("no-fullscreen")));
    const char* dangling[] = {"app", "-platform"};
    QVERIFY(platformOptionsFromArguments(2, dangling, QString()).isEmpty());
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

  void hiddenControlsCloseUpOnQnx() {
    for (const auto controls : {ControlsLayout::MacOS, ControlsLayout::Windows}) {
      WindowChromeConfig config;
      config.controls = controls;
      const auto layout = layoutChrome(config, capabilitiesForPlatform(QStringLiteral("qnx")), 1920);
      QCOMPARE(actions(layout), std::vector<WindowAction>{WindowAction::Close});
      QCOMPARE(layout.hidden.size(), size_t(2));
      for (const auto& hidden : layout.hidden) QVERIFY(!hidden.reason.isEmpty());
      if (controls == ControlsLayout::MacOS)
        QCOMPARE(layout.controls.front().hitRect.left(), kEdgePadding);
      else
        QCOMPARE(layout.controls.front().hitRect.right(), 1920.0);
    }
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
