// The beacon prints a RNQNX_NODE line for each watched node once it is visible
// and a frame has been presented. Host evidence only; no QNX Screen involved.

#include "NodeBeacon.h"

#include "WindowChrome.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

using rnqnx::window::NodeBeacon;

namespace {

// A window whose root holds `shown` (visible) and `hidden` (invisible) nodes.
// `late` is created by a Loader that activates after the first frames.
constexpr const char* kScene = R"qml(
import QtQuick
Item {
    objectName: "sceneRoot"
    width: 200; height: 100
    Rectangle { objectName: "shown"; width: 50; height: 50; color: "red" }
    Item {
        visible: false
        Rectangle { objectName: "hiddenByParent"; width: 10; height: 10 }
    }
    Loader {
        id: loader
        active: false
        sourceComponent: Rectangle { objectName: "late"; width: 10; height: 10 }
    }
    function activate() { loader.active = true }
}
)qml";

struct Scene {
  Scene() {
    window = std::make_unique<QQuickWindow>();
    window->resize(200, 100);
    QQmlComponent component(&engine);
    component.setData(kScene, QUrl());
    root.reset(qobject_cast<QQuickItem*>(component.create()));
    if (root) root->setParentItem(window->contentItem());
  }
  ~Scene() {
    root.reset();
    window.reset();
  }
  QQmlEngine engine;
  std::unique_ptr<QQuickWindow> window;
  std::unique_ptr<QQuickItem> root;
};

}  // namespace

class NodeBeaconTest : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void reportsAVisibleNodeOnceAfterAFrame() {
    Scene scene;
    QVERIFY(scene.root);
    QStringList lines;
    NodeBeacon beacon(*scene.window, *scene.root, {QStringLiteral("shown")},
                      [&lines](const QString& line) { lines.append(line); });
    // No frame has been presented yet.
    QCOMPARE(lines.size(), 0);
    scene.window->show();
    QTRY_COMPARE(lines.size(), 1);
    QCOMPARE(lines.at(0), QStringLiteral("RNQNX_NODE name=shown visible=1"));
    QVERIFY(beacon.pending().isEmpty());
    // Later frames add nothing.
    scene.root->update();
    QTest::qWait(100);
    QCOMPARE(lines.size(), 1);
  }

  void staysSilentForANodeThatIsNotThere() {
    Scene scene;
    QStringList lines;
    NodeBeacon beacon(*scene.window, *scene.root,
                      {QStringLiteral("noSuchNode"), QStringLiteral("hiddenByParent"),
                       QStringLiteral("shown")},
                      [&lines](const QString& line) { lines.append(line); });
    scene.window->show();
    QTRY_COMPARE(lines.size(), 1);
    QTest::qWait(100);
    QCOMPARE(lines, (QStringList{QStringLiteral("RNQNX_NODE name=shown visible=1")}));
    QCOMPARE(beacon.pending(),
             (QStringList{QStringLiteral("noSuchNode"), QStringLiteral("hiddenByParent")}));
  }

  void reportsANodeThatAppearsLater() {
    Scene scene;
    QStringList lines;
    NodeBeacon beacon(*scene.window, *scene.root,
                      {QStringLiteral("late"), QStringLiteral("shown")},
                      [&lines](const QString& line) { lines.append(line); });
    scene.window->show();
    QTRY_COMPARE(lines.size(), 1);
    QCOMPARE(lines.at(0), QStringLiteral("RNQNX_NODE name=shown visible=1"));
    QVERIFY(QMetaObject::invokeMethod(scene.root.get(), "activate"));
    scene.root->update();
    QTRY_COMPARE(lines.size(), 2);
    QCOMPARE(lines.at(1), QStringLiteral("RNQNX_NODE name=late visible=1"));
  }

  void rootCanMatch() {
    Scene scene;
    QStringList lines;
    NodeBeacon beacon(*scene.window, *scene.root, {QStringLiteral("sceneRoot")},
                      [&lines](const QString& line) { lines.append(line); });
    scene.window->show();
    QTRY_COMPARE(lines.size(), 1);
    QCOMPARE(lines.at(0), QStringLiteral("RNQNX_NODE name=sceneRoot visible=1"));
  }

  void destroyedBeaconIsNotCalledAgain() {
    Scene scene;
    int calls = 0;
    {
      NodeBeacon beacon(*scene.window, *scene.root, {QStringLiteral("late")},
                        [&calls](const QString&) { ++calls; });
    }
    scene.window->show();
    QVERIFY(QTest::qWaitForWindowExposed(scene.window.get()));
    QVERIFY(QMetaObject::invokeMethod(scene.root.get(), "activate"));
    scene.root->update();
    QTest::qWait(200);
    QCOMPARE(calls, 0);
  }

  // The real chrome: the beacon names the nodes the Pi smoke path waits for.
  void reportsTheRealChromeNodes() {
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(800, 600);
    rnqnx::window::WindowChrome chrome(
        window, engine, {},
        rnqnx::window::capabilitiesForPlatform(QStringLiteral("cocoa"), {},
                                               rnqnx::window::SurfaceResize::FollowsWindow));
    QStringList lines;
    NodeBeacon beacon(window, *chrome.chromeItem(),
                      {QStringLiteral("rnqnxWindowChrome"), QStringLiteral("rnqnxTitleBar")},
                      [&lines](const QString& line) { lines.append(line); });
    window.show();
    QTRY_COMPARE(lines.size(), 2);
    QVERIFY(lines.contains(QStringLiteral("RNQNX_NODE name=rnqnxWindowChrome visible=1")));
    QVERIFY(lines.contains(QStringLiteral("RNQNX_NODE name=rnqnxTitleBar visible=1")));
  }
};

QTEST_MAIN(NodeBeaconTest)
#include "NodeBeaconTest.moc"
