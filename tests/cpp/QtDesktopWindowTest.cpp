#include "react-native-qnx/views/QtDesktopWindow.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <QVariant>

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

void waitFor(const std::function<bool()>& ready, const char* message) {
  QElapsedTimer deadline;
  deadline.start();
  while (!ready() && deadline.elapsed() < 3000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QThread::msleep(1);
  }
  require(ready(), message);
}

void settle() {
  QElapsedTimer deadline;
  deadline.start();
  while (deadline.elapsed() < 30) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QThread::msleep(1);
  }
}

QQuickItem& itemNamed(QQuickItem& root, const char* name) {
  auto* item = root.findChild<QQuickItem*>(QString::fromLatin1(name));
  require(item != nullptr, std::string("Missing desktop item: ") + name);
  return *item;
}

bool beneath(QQuickItem* item, const QQuickItem& ancestor) {
  while (item != nullptr) {
    if (item == &ancestor) return true;
    item = item->parentItem();
  }
  return false;
}

void clickAt(QQuickWindow& window, QQuickItem& item, QPointF local) {
  const QPointF scene = item.mapToScene(local);
  const QPointF global = window.mapToGlobal(scene.toPoint());
  QMouseEvent press(QEvent::MouseButtonPress, scene, global,
      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QMouseEvent release(QEvent::MouseButtonRelease, scene, global,
      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&window, &press);
  QCoreApplication::sendEvent(&window, &release);
  settle();
}

void click(QQuickWindow& window, QQuickItem& item) {
  clickAt(window, item, QPointF(item.width() / 2, item.height() / 2));
}

void key(QQuickWindow& window, int code,
    Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent press(QEvent::KeyPress, code, modifiers);
  QKeyEvent release(QEvent::KeyRelease, code, modifiers);
  QCoreApplication::sendEvent(&window, &press);
  QCoreApplication::sendEvent(&window, &release);
  settle();
}

void openFileMenu(QQuickWindow& window, QQuickItem& frame,
    int activationKey = Qt::Key_Space) {
  auto& menuBar = itemNamed(frame, "rnqnxMenuBar");
  key(window, Qt::Key_F10);
  waitFor([&] { return beneath(window.activeFocusItem(), menuBar); },
      "F10 did not focus the menu bar");
  key(window, activationKey);
  auto* menu = frame.findChild<QObject*>(QStringLiteral("rnqnxFileMenu"));
  require(menu != nullptr, "Missing File menu");
  waitFor([&] { return menu->property("visible").toBool(); },
      "Keyboard activation did not open the focused File menu");
}

std::unique_ptr<QQuickItem> createClient(QQmlEngine& engine, QQuickItem& viewport) {
  QQmlComponent component(&engine);
  component.setData(R"QML(
import QtQuick
Rectangle {
    id: client
    anchors.fill: parent
    color: "#ffffff"
    property int pressCount: 0
    Rectangle {
        objectName: "desktopTestTarget"
        x: parent.width - width - 24
        y: 24
        width: 200
        height: 48
        radius: 6
        color: "#175cd3"
        Text { anchors.centerIn: parent; text: "Test client button"; color: "white" }
        MouseArea { anchors.fill: parent; onClicked: client.pressCount += 1 }
    }
    Text {
        x: 24
        y: 100
        text: "Native desktop regression\nQt host test client"
        color: "#233146"
        font.pixelSize: 20
    }
}
)QML", QUrl(QStringLiteral("desktop-test-client.qml")));
  std::unique_ptr<QObject> object(component.create());
  auto* item = qobject_cast<QQuickItem*>(object.get());
  require(item != nullptr, component.errorString().toStdString());
  std::unique_ptr<QQuickItem> client(item);
  object.release();
  client->setParentItem(&viewport);
  return client;
}

double clientFrameInteractions() {
  QStringList qmlWarnings;
  QQmlEngine engine;
  QObject::connect(&engine, &QQmlEngine::warnings, &engine,
      [&](const QList<QQmlError>& warnings) {
        for (const auto& warning : warnings) qmlWarnings.append(warning.toString());
      });
  QQuickWindow window;
  window.setTitle(QStringLiteral("React Native QNX — desktop host test"));
  window.resize(640, 480);
  rnqnx::views::QtDesktopWindow desktop(engine, window);
  auto& viewport = desktop.contentItem();
  auto* frame = viewport.parentItem();
  require(frame != nullptr, "Desktop client has no frame");
  require(frame->property("appWindow").value<QObject*>() == &window,
      "QML desktop did not retain the actual native window");
  auto& initialMenuBar = itemNamed(*frame, "rnqnxMenuBar");
  require(viewport.width() > 0 && viewport.height() > 0 && initialMenuBar.height() > 0 &&
      std::abs(viewport.x() - 8) < 0.01 && std::abs(viewport.width() - 624) < 0.01 &&
      std::abs(viewport.y() - (initialMenuBar.y() + initialMenuBar.height())) < 0.01 &&
      std::abs(viewport.height() - (480 - viewport.y() - 8)) < 0.01,
      "Client geometry is not ready before the first event-loop turn");
  window.resize(680, 520);
  require(std::abs(viewport.width() - 664) < 0.01 &&
      std::abs(viewport.height() - (520 - viewport.y() - 8)) < 0.01,
      "Client geometry did not follow resize before the first show");
  window.resize(640, 480);
  require(std::abs(viewport.width() - 624) < 0.01,
      "Pre-show geometry did not return to the configured window size");
  auto client = createClient(engine, viewport);
  auto& target = itemNamed(*client, "desktopTestTarget");
  auto& minimize = itemNamed(*frame, "rnqnxMinimizeButton");
  auto& maximize = itemNamed(*frame, "rnqnxMaximizeButton");
  auto& close = itemNamed(*frame, "rnqnxCloseButton");
  require(window.flags().testFlag(Qt::FramelessWindowHint),
      "Client frame did not request a frameless native window");
  window.show();
  window.requestActivate();
  waitFor([&] { return window.isActive() && viewport.width() > 0; },
      "Desktop window did not become active");
  const double clientInset = viewport.y();
  require(clientInset > 48 && viewport.isEnabled(),
      "Client viewport did not begin below the title and menu bars");

  click(window, target);
  require(client->property("pressCount").toInt() == 1,
      "Real window mouse event did not reach the client fixture");
  click(window, maximize);
  waitFor([&] { return window.visibility() == QWindow::Maximized; },
      "Maximize control did not maximize the native window");
  click(window, maximize);
  waitFor([&] { return window.visibility() == QWindow::Windowed; },
      "Restore control did not restore the native window");
  key(window, Qt::Key_F11);
  waitFor([&] { return window.visibility() == QWindow::FullScreen; },
      "F11 did not enter full screen");
  key(window, Qt::Key_F11);
  waitFor([&] { return window.visibility() == QWindow::Windowed; },
      "F11 did not leave full screen");

  window.resize(720, 540);
  waitFor([&] {
    return std::abs(viewport.width() - 704) < 0.01 &&
        std::abs(viewport.height() - (540 - viewport.y() - 8)) < 0.01;
  }, "Client viewport did not follow native window resize");
  require(std::abs(viewport.y() - clientInset) < 0.01,
      "Window resize changed the title/menu inset");

  openFileMenu(window, *frame);
  require(!viewport.isEnabled(), "An open menu left client input enabled");
  auto* fileMenu = frame->findChild<QObject*>(QStringLiteral("rnqnxFileMenu"));
  key(window, Qt::Key_Right);
  waitFor([&] {
    return frame->property("popupVisible").toBool() &&
        !fileMenu->property("visible").toBool();
  }, "Right arrow did not navigate to the next open menu");
  key(window, Qt::Key_Left);
  waitFor([&] { return fileMenu->property("visible").toBool(); },
      "Left arrow did not navigate back to the File menu");
  key(window, Qt::Key_Escape);
  waitFor([&] { return !frame->property("popupVisible").toBool(); },
      "Escape did not dismiss the menu");
  require(viewport.isEnabled(), "Escape did not restore client input");

  for (const int activation : {Qt::Key_Down, Qt::Key_Return, Qt::Key_Enter}) {
    openFileMenu(window, *frame, activation);
    require(!viewport.isEnabled(), "Keyboard-opened menu left client input enabled");
    key(window, Qt::Key_Escape);
    waitFor([&] { return viewport.isEnabled(); },
        "Escape did not release the keyboard-opened menu");
  }

  openFileMenu(window, *frame);
  // The right edge of this client button is outside both the File popup and
  // the centered dialog. The opening press must never reach client content.
  clickAt(window, target, QPointF(target.width() - 8, target.height() / 2));
  require(client->property("pressCount").toInt() == 1,
      "Click outside an open menu activated underlying client content");
  if (frame->property("popupVisible").toBool()) key(window, Qt::Key_Escape);
  waitFor([&] { return viewport.isEnabled(); },
      "Client input did not resume after dismissing the menu");

  key(window, Qt::Key_F1);
  waitFor([&] {
    auto* dialog = frame->findChild<QObject*>(QStringLiteral("rnqnxDesktopDialog"));
    return dialog != nullptr && dialog->property("visible").toBool();
  }, "F1 did not open the keyboard-help dialog");
  require(!viewport.isEnabled(), "A modal dialog left client input enabled");
  clickAt(window, target, QPointF(target.width() - 8, target.height() / 2));
  require(client->property("pressCount").toInt() == 1,
      "Click outside a modal dialog activated underlying client content");
  // Outside-click dismissal is allowed; Escape must also close an open dialog.
  if (frame->property("dialogKind").toString().isEmpty()) {
    key(window, Qt::Key_F1);
    waitFor([&] { return !viewport.isEnabled(); }, "Help dialog did not reopen");
  }
  key(window, Qt::Key_Escape);
  waitFor([&] { return frame->property("dialogKind").toString().isEmpty(); },
      "Escape did not close the help dialog");
  require(viewport.isEnabled(), "Dialog dismissal trapped client input");
  click(window, target);
  require(client->property("pressCount").toInt() == 2,
      "Client control stopped working after menu/dialog dismissal");

  // Simulate the capability supplied on QNX. This proves both the visible
  // control and keyboard Action honor the gate, not QNX window-manager support.
  require(frame->setProperty("nativeMinimizeAvailable", false),
      "Could not set the native minimize capability");
  require(!minimize.isEnabled(), "Unavailable minimize control stayed enabled");
  click(window, minimize);
  key(window, Qt::Key_M, Qt::ControlModifier);
  require(window.visibility() == QWindow::Windowed,
      "Unavailable minimize action changed native window visibility");

  const QString screenshot = qEnvironmentVariable("RNQNX_DESKTOP_TEST_SCREENSHOT");
  if (!screenshot.isEmpty()) {
    QImage image;
    waitFor([&] { image = window.grabWindow(); return !image.isNull(); },
        "Desktop window did not produce a rendered frame");
    require(image.save(screenshot), "Could not save the desktop regression frame");
  }
  require(qmlWarnings.isEmpty(), qmlWarnings.join('\n').toStdString());
  click(window, close);
  waitFor([&] { return !window.isVisible(); },
      "Close control did not close the native window");
  return clientInset;
}

void systemFramePreservesNativeDecorations(double clientInset) {
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(640, 480);
  const auto originalFlags = window.flags();
  rnqnx::views::QtDesktopWindow desktop(
      engine, window, rnqnx::views::WindowFrame::System);
  require(window.flags() == originalFlags &&
      !window.flags().testFlag(Qt::FramelessWindowHint),
      "System frame changed native decoration flags");
  auto& viewport = desktop.contentItem();
  auto* frame = viewport.parentItem();
  require(frame != nullptr, "System-frame client has no frame item");
  require(viewport.width() > 0 && viewport.height() > 0 &&
      std::abs(viewport.width() - 640) < 0.01 &&
      std::abs(viewport.y() - itemNamed(*frame, "rnqnxMenuBar").height()) < 0.01 &&
      std::abs(viewport.height() - (480 - viewport.y())) < 0.01,
      "System-frame client geometry is not ready before the first event-loop turn");
  window.show();
  window.requestActivate();
  waitFor([&] { return window.isActive() && viewport.width() > 0; },
      "System-frame window did not become active");
  require(!itemNamed(*frame, "rnqnxMinimizeButton").isVisible() &&
      !itemNamed(*frame, "rnqnxMaximizeButton").isVisible() &&
      !itemNamed(*frame, "rnqnxCloseButton").isVisible(),
      "System frame displayed duplicate custom title controls");
  int hiddenHandles = 0;
  auto remaining = frame->childItems();
  while (!remaining.isEmpty()) {
    auto* item = remaining.takeLast();
    remaining.append(item->childItems());
    if (item->property("edges").isValid()) {
      require(!item->isVisible(), "System frame displayed a custom resize handle");
      ++hiddenHandles;
    }
  }
  require(hiddenHandles == 8, "Did not inspect all system-frame resize handles");
  require(viewport.y() > 0 && viewport.y() < clientInset &&
      std::abs(viewport.y() - itemNamed(*frame, "rnqnxMenuBar").height()) < 0.01,
      "System frame did not retain only the menu-bar client inset");
  require(std::abs(viewport.width() - 640) < 0.01 &&
      std::abs(viewport.height() - (480 - viewport.y())) < 0.01,
      "System-frame client has an unexpected custom border inset");
  openFileMenu(window, *frame);
  require(!viewport.isEnabled(), "System-frame menu did not block client input");
  key(window, Qt::Key_Escape);
  waitFor([&] { return viewport.isEnabled(); },
      "System-frame menu did not restore client input");
  key(window, Qt::Key_W, Qt::ControlModifier);
  waitFor([&] { return !window.isVisible(); },
      "Close shortcut did not close a system-frame window");
}

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  application.setQuitOnLastWindowClosed(false);
  try {
    const double clientInset = clientFrameInteractions();
    systemFramePreservesNativeDecorations(clientInset);
    std::cout << "RNQNX_DESKTOP_WINDOW_OK mouse keyboard popup-blocking resize capability system-frame\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_DESKTOP_WINDOW_FAIL " << error.what() << '\n';
    return 1;
  }
}
