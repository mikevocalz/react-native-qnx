// Generic non-premium Qt/Fabric interaction driver. The default fixture tests
// public RN primitives; an external consumer supplies its own package bundle.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/runtime/QnxExceptionsManager.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "../../vnext/probes/playground/QmlSources.h"
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
QQuickItem* findItem(QQuickItem* root, const QString& testId) {
  if (root->property("rnqnxTestId").toString() == testId) return root;
  for (auto* child : root->childItems()) if (auto* found = findItem(child, testId)) return found;
  return nullptr;
}
bool visibleItem(QQuickItem* item, const QQuickWindow& window) {
  if (!item || !item->isVisible() || item->width() <= 0 || item->height() <= 0) return false;
  QRectF visible = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
  visible = visible.intersected(QRectF(0, 0, window.width(), window.height()));
  for (auto* parent = item; parent; parent = parent->parentItem()) {
    if (parent->opacity() <= 0) return false;
    if (parent->clip()) visible = visible.intersected(parent->mapRectToScene(QRectF(0, 0, parent->width(), parent->height())));
  }
  return !visible.isEmpty();
}
int keyCode(const std::string& name) {
  if (name == "Enter") return Qt::Key_Return;
  if (name == "Space") return Qt::Key_Space;
  if (name == "Tab") return Qt::Key_Tab;
  if (name == "Backtab") return Qt::Key_Backtab;
  if (name == "Escape") return Qt::Key_Escape;
  if (name == "ContextMenu") return Qt::Key_Menu;
  if (name == "ArrowDown") return Qt::Key_Down;
  if (name == "ArrowUp") return Qt::Key_Up;
  if (name == "ArrowLeft") return Qt::Key_Left;
  if (name == "ArrowRight") return Qt::Key_Right;
  if (name == "Home") return Qt::Key_Home;
  if (name == "End") return Qt::Key_End;
  if (name == "Backspace") return Qt::Key_Backspace;
  if (name.size() == 1) return QString::fromStdString(name).toUpper().at(0).unicode();
  throw std::runtime_error("Unsupported probe key: " + name);
}

void run(const QString& path, const QString& appName, const QString& screenshot,
         const QString& expectedPlatform) {
  using namespace rnqnx::probes::qml;
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(960, 720);
  window.setColor(Qt::white);
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType("View", QByteArray(kViewSource));
  factory.registerType("Text", QByteArray(kTextSource));
  factory.registerType("Image", QByteArray(kImageSource));
  factory.registerType("ScrollView", QByteArray(kScrollViewSource));
  factory.registerType("TextInput", QByteArray(kTextInputSource));
  factory.registerType("ActivityIndicatorView", QByteArray(kActivityIndicatorSource));
  factory.registerType("Switch", QByteArray(kSwitchSource));
  rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 11);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  for (const char* name : {"View", "Image", "ScrollView", "TextInput", "ActivityIndicatorView", "Switch"})
    adapter.mapComponent(name, name);
  adapter.mapComponent("Paragraph", "Text");
  adapter.mapComponent("SafeAreaView", "View");
  adapter.mapComponent("ModalHostView", "View");
  rnqnx::fabric::QtTouchDispatcher dispatcher(layer, *window.contentItem());
  adapter.setTouchDispatcher(&dispatcher);
  window.installEventFilter(&dispatcher);
  rnqnx::runtime::QtReactHost host(adapter);
  host.delegate().setTouchDispatcher(&dispatcher);
  host.installTurboModuleBinding();
  host.installComponentProvider();
  host.installPlatformConstants();
  host.installTimers();
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Cannot open probe bundle");
  require(host.evaluate(file.readAll().toStdString(), path.toStdString()), "Probe evaluation: " + host.lastError().toStdString());
  auto& rt = host.runtime();
  const auto platform = rt.global().getProperty(rt, "__qtProbePlatform");
  require(platform.isString() && platform.asString(rt).utf8(rt) == expectedPlatform.toStdString(),
      "Fixture Platform.OS does not match expected target " + expectedPlatform.toStdString());
  window.show();
  window.requestActivate();
  host.startSurface(11, appName.toStdString(), 960, 720);
  const auto pump = [&]() {
    rt.drainMicrotasks();
    host.pumpEvents();
    require(host.lastError().isEmpty(), host.lastError().toStdString());
    if (!host.javaScriptErrors().empty()) {
      const auto& error = host.javaScriptErrors().front();
      throw std::runtime_error("React JavaScript error: " + error.message + "\n" + error.stack);
    }
    const auto& failures = host.delegate().lastFailures();
    require(failures.empty(), failures.empty() ? "" : failures.front().detail.toStdString());
  };
  const auto until = [&](auto condition, const std::string& label) {
    QElapsedTimer elapsed;
    elapsed.start();
    do {
      pump();
      if (condition()) return;
      QThread::msleep(1);
    } while (elapsed.elapsed() < 5000);
    std::string snapshot;
    const auto snapshotValue = rt.global().getProperty(rt, "__qtProbeSnapshot");
    if (snapshotValue.isObject() && snapshotValue.asObject(rt).isFunction(rt))
      snapshot = snapshotValue.asObject(rt).asFunction(rt).call(rt).asString(rt).utf8(rt);
    throw std::runtime_error("Probe timed out: " + label + " state=" + snapshot);
  };
  until([&]() { return layer.mountedCount() > 1; }, "initial React mount");
  const auto steps = rt.global().getPropertyAsObject(rt, "__qtProbeSteps").asArray(rt);
  const auto check = rt.global().getPropertyAsFunction(rt, "__qtProbeCheck");
  for (std::size_t index = 0; index < steps.size(rt); ++index) {
    const auto step = steps.getValueAtIndex(rt, index).asObject(rt);
    const auto string = [&](const char* name) {
      const auto value = step.getProperty(rt, name);
      return value.isString() ? value.asString(rt).utf8(rt) : std::string();
    };
    const auto action = string("action");
    const auto label = string("label");
    const auto targetId = QString::fromStdString(string("target"));
    QPointer<QQuickItem> target;
    if (!targetId.isEmpty()) {
      until([&]() { target = findItem(window.contentItem(), targetId); return visibleItem(target, window); }, label + " target");
    }
    if (action == "hover" || action == "click") {
      require(target != nullptr, "Pointer step requires target");
      const QPointF point = target->mapToScene(QPointF(target->width()/2, target->height()/2));
      const auto mouse = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, point, window.mapToGlobal(point), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &event);
      };
      mouse(QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
      if (action == "click") {
        mouse(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
        pump();
        mouse(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
      }
    } else if (action == "leave") {
      QEvent event(QEvent::Leave); QCoreApplication::sendEvent(&window, &event);
    } else if (action == "focus") {
      require(target != nullptr, "Focus step requires target");
      target->forceActiveFocus(Qt::OtherFocusReason);
    } else if (action == "key" || action == "keyDown" || action == "keyUp") {
      if (target) {
        target->forceActiveFocus(Qt::OtherFocusReason);
        until([&]() { return target && (target->hasActiveFocus() || target->isAncestorOf(window.activeFocusItem())); }, label + " keyboard focus");
      }
      const auto name = string("key");
      const int code = keyCode(name);
      const auto modifiers = name == "Backtab" ? Qt::ShiftModifier : Qt::NoModifier;
      const QString text = QString::fromStdString(string("text"));
      if (action != "keyUp") {
        QKeyEvent down(QEvent::KeyPress, code, modifiers, text);
        QCoreApplication::sendEvent(&window, &down); pump();
      }
      if (action != "keyDown") {
        QKeyEvent up(QEvent::KeyRelease, code, modifiers, text);
        QCoreApplication::sendEvent(&window, &up);
      }
    } else if (action == "call") {
      rt.global().getPropertyAsFunction(rt, "__qtProbeAction").call(rt, static_cast<double>(index));
    } else {
      require(action == "assert", "Unknown probe action: " + action);
    }
    const auto visible = QString::fromStdString(string("visible"));
    const auto hidden = QString::fromStdString(string("hidden"));
    until([&]() {
      if (!visible.isEmpty() && !visibleItem(findItem(window.contentItem(), visible), window)) return false;
      if (!hidden.isEmpty()) { auto* item = findItem(window.contentItem(), hidden); if (item && item->isVisible()) return false; }
      return check.call(rt, static_cast<double>(index)).getBool();
    }, label);
    std::cout << "QT_CONTROL_STEP " << index << ' ' << label << '\n';
    if (!screenshot.isEmpty() && !visible.isEmpty()) {
      window.requestUpdate(); pump();
      require(window.grabWindow().save(screenshot + "." + QString::number(index) + ".png"), "Step screenshot capture failed");
    }
  }
  if (!screenshot.isEmpty()) {
    window.requestUpdate(); pump();
    require(window.grabWindow().save(screenshot), "Screenshot capture failed");
  }
  host.stopSurface(11);
  until([&]() { return layer.mountedCount() == 1; }, "surface teardown");
  std::cout << "QT_CONTROLS_FABRIC_OK steps=" << steps.size(rt)
      << " platform=" << expectedPlatform.toStdString()
      << " renderer=Fabric input=synthetic-Qt\n";
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addOption({"bundle", "Production Metro fixture", "path", RNQNX_DESKTOP_INPUT_BUNDLE});
  parser.addOption({"app", "Registered React component", "name", "RNQnxDesktopInputProbe"});
  parser.addOption({"screenshot", "PNG after completed assertions", "path"});
  parser.addOption({"expected-platform", "Public React Native platform identity", "name", "qnx"});
  parser.process(app);
  try { run(parser.value("bundle"), parser.value("app"), parser.value("screenshot"), parser.value("expected-platform")); }
  catch (const std::exception& error) { std::cerr << "QT_CONTROLS_FABRIC_ERROR " << error.what() << '\n'; return 1; }
  return 0;
}
