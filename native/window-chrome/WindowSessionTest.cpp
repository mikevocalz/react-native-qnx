#include "WindowSession.h"
#include "WindowHost.h"
#include <QCloseEvent>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <limits>
#include <thread>

using namespace rnqnx::window;
class WindowSessionTest final : public QObject {
  Q_OBJECT
 private Q_SLOTS:
  void commandsObserveActualWindowAndRejectInvalidValues() {
    QQuickWindow window;
    window.resize(640, 480);
    auto caps = capabilitiesForPlatform(QStringLiteral("offscreen"), {}, SurfaceResize::FollowsWindow);
    WindowSession session(window, nullptr, caps);
    session.setTitle(QStringLiteral("Document"));
    session.setSize(700, 500);
    session.setPosition(20, 30);
    const auto state = session.state();
    QCOMPARE(state->snapshot.value("title").toString(), QStringLiteral("Document"));
    QCOMPARE(state->snapshot.value("width").toInt(), 700);
    QCOMPARE(state->snapshot.value("height").toInt(), 500);
    QCOMPARE(state->snapshot.value("x").toInt(), 20);
    QCOMPARE(state->snapshot.value("y").toInt(), 30);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, session.setSize(0, 400));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, session.setSize(100.5, 400));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, session.setSize(std::numeric_limits<double>::quiet_NaN(), 400));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, session.setPosition(std::numeric_limits<double>::infinity(), 0));
    QCOMPARE(window.size(), QSize(700, 500));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, session.setControlsLayout("windows"));
  }
  void nativeCloseGuardsVetoEveryClosePathAndAreIndependent() {
    QQuickWindow window;
    window.show();
    auto caps = capabilitiesForPlatform(QStringLiteral("offscreen"));
    WindowSession session(window, nullptr, caps);
    auto first = session.state()->guard();
    auto second = session.state()->guard();
    QVERIFY(!session.close());
    QVERIFY(window.isVisible());
    QVERIFY(session.state()->snapshot.value("closeRequested").toBool());
    const auto cancelledId = session.state()->snapshot.value("closeRequestId").toString();
    QVERIFY(!cancelledId.isEmpty());
    QVERIFY(session.cancelCloseRequest(cancelledId));
    session.setTitle("After cancelling the dialog");
    QVERIFY(!session.state()->snapshot.value("closeRequested").toBool());
    QVERIFY(session.state()->blocked());
    first->store(false);
    QVERIFY(!session.close());
    QVERIFY(!session.cancelCloseRequest(cancelledId));
    QVERIFY(session.state()->snapshot.value("closeRequested").toBool());
    QVERIFY(session.state()->snapshot.value("closeRequestId").toString() != cancelledId);
    QCloseEvent external;
    QCoreApplication::sendEvent(&window, &external);
    QVERIFY(!external.isAccepted());
    second->store(false);
    QVERIFY(window.isVisible()); // Removing guards never closes by itself.
    QVERIFY(session.close());
    QVERIFY(!window.isVisible());
  }
  void expiredGuardDoesNotPermanentlyVeto() {
    QQuickWindow window;
    WindowSession session(window, nullptr, capabilitiesForPlatform("offscreen"));
    { auto guard = session.state()->guard(); QVERIFY(session.state()->blocked()); }
    QVERIFY(!session.state()->blocked());
  }
  void queueRunsOnGuiOwnerAndShutdownRejectsPending() {
    QQuickWindow window;
    WindowSession session(window, nullptr, capabilitiesForPlatform("offscreen"));
    auto state = session.state();
    std::atomic<bool> rejected{false}, executed{false};
    std::thread worker([&] {
      QVERIFY(state->submit([&](WindowSession& owner) {
        QVERIFY(QThread::currentThread() == owner.thread());
        owner.setTitle("From worker");
        executed = true;
      }, [&] { rejected = true; }));
    });
    worker.join();
    QVERIFY(!executed.load());
    QTRY_VERIFY(executed.load());
    QVERIFY(!rejected.load());
    QVERIFY(state->submit([&](WindowSession&) { QFAIL("pending command ran after shutdown"); },
                          [&] { rejected = true; }));
    session.shutdown();
    QVERIFY(rejected.load());
    QVERIFY(!state->submit([](WindowSession&) {}, [] {}));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, state->guard());
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, state->listen([](auto, const auto&, bool) {}));
    QCoreApplication::processEvents();
  }
  void observersHaveInitialSnapshotAndOrderedChangesAndCleanup() {
    QQuickWindow window;
    WindowSession session(window, nullptr, capabilitiesForPlatform("offscreen"));
    std::vector<std::uint64_t> versions;
    QStringList titles;
    auto state = session.state();
    const auto id = state->listen([&](auto version, const auto& value, bool) {
      versions.push_back(version);
      titles.push_back(value.value("title").toString());
    });
    QCOMPARE(versions.size(), std::size_t(1));
    session.setTitle("First");
    session.setTitle("Second");
    QCOMPARE(titles.last(), QStringLiteral("Second"));
    for (std::size_t i = 1; i < versions.size(); ++i) QCOMPARE(versions[i], versions[i - 1] + 1);
    state->unlisten(id);
    state->unlisten(id);
    const auto count = titles.size();
    session.setTitle("After cleanup");
    QCOMPARE(titles.size(), count);
  }
  void hiddenChromeInsetChangesWithNativeControlLayout() {
    QQuickWindow window;
    window.resize(800, 600);
    QQmlEngine engine;
    WindowChromeConfig config;
    config.titleBarStyle = TitleBarStyle::Hidden;
    auto caps = capabilitiesForPlatform("cocoa", {}, SurfaceResize::FollowsWindow);
    WindowChrome chrome(window, engine, config, caps);
    WindowSession session(window, &chrome, caps);
    const auto before = session.state()->snapshot.value("insets").toMap();
    QCOMPARE(before.value("top").toDouble(), 44.0);
    session.setControlsLayout("windows");
    QCOMPARE(session.state()->snapshot.value("controlsLayout").toString(), QStringLiteral("windows"));
    const auto after = session.state()->snapshot.value("insets").toMap();
    QVERIFY(after.value("overlay").toMap().value("x").toDouble() >
            before.value("overlay").toMap().value("x").toDouble());
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, session.setControlsLayout("invalid"));
  }
  void contentOffsetAndInsetsDoNotDoubleCountTheBar() {
    for (const auto style : {TitleBarStyle::Default, TitleBarStyle::Hidden}) {
      QQuickWindow window;
      window.resize(800, 600);
      QQmlEngine engine;
      WindowChromeConfig config;
      config.titleBarStyle = style;
      auto caps = capabilitiesForPlatform("cocoa", {}, SurfaceResize::FollowsWindow);
      WindowChrome chrome(window, engine, config, caps);
      WindowSession session(window, &chrome, caps);
      const auto snapshot = session.state()->snapshot;
      const auto inset = snapshot.value("insets").toMap().value("top").toDouble();
      QCOMPARE(chrome.contentRoot().y() + inset, 44.0);
      QCOMPARE(snapshot.value("contentHeight").toDouble() + chrome.contentRoot().y(), 600.0);
      QCOMPARE(snapshot.value("contentWidth").toDouble(), 800.0);
    }
  }
  void repeatedGuardCleanupIsBounded() {
    QQuickWindow window;
    WindowSession session(window, nullptr, capabilitiesForPlatform("offscreen"));
    for (int i = 0; i < 1000; ++i) {
      auto guard = session.state()->guard();
      guard->store(false);
    }
    QVERIFY(session.state()->guards.size() <= 1);
  }
  void waylandVariantsRejectPositionAndResize() {
    for (const auto& name : {QStringLiteral("wayland"), QStringLiteral("wayland-egl")}) {
      QQuickWindow window;
      window.resize(640, 480);
      WindowSession session(window, nullptr, capabilitiesForPlatform(name));
      QVERIFY(!session.state()->snapshot.value("canResize").toBool());
      QVERIFY(!session.state()->snapshot.value("canSetPosition").toBool());
      QVERIFY_THROWS_EXCEPTION(std::runtime_error, session.setSize(800, 600));
      QVERIFY_THROWS_EXCEPTION(std::runtime_error, session.setPosition(10, 20));
      QCOMPARE(window.size(), QSize(640, 480));
    }
  }
  void explicitManagerCloseVetoCapability() {
    QVERIFY(!managerReplySupportsCloseVeto("rnqnx-winmgr windowed"));
    QVERIFY(!managerReplySupportsCloseVeto("fullscreen-winmgr close-veto"));
    QVERIFY(!managerReplySupportsCloseVeto("other windowed close-veto"));
    QVERIFY(managerReplySupportsCloseVeto("rnqnx-winmgr windowed close-veto"));
    QVERIFY(managerReplySupportsCloseVeto("rnqnx-winmgr windowed close-veto future"));
  }
  void unavailableCapabilitiesFailWithoutChangingGeometry() {
    QQuickWindow window;
    window.resize(640, 480);
    auto caps = capabilitiesForPlatform("qnx");
    caps.containedWindow = true;
    caps.minimize.available = false;
    WindowSession session(window, nullptr, caps);
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, session.setSize(800, 600));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, session.setPosition(10, 10));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, session.minimize());
    QCOMPARE(window.size(), QSize(640, 480));
  }
};
QTEST_MAIN(WindowSessionTest)
#include "WindowSessionTest.moc"
