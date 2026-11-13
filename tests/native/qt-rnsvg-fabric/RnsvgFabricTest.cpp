// react-native-svg 15.16.0, unmodified, through Metro, Hermes, Fabric, the Qt
// mounting layer and the touch dispatcher. Pixels come from the real window.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <RnsvgItems.h>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>

#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

rnqnx::views::RnsvgRootItem* findRoot(QQuickItem* parent) {
  if (auto* item = qobject_cast<rnqnx::views::RnsvgRootItem*>(parent)) return item;
  for (auto* child : parent->childItems()) {
    if (auto* item = findRoot(child)) return item;
  }
  return nullptr;
}

int countElements(const QQuickItem* parent) {
  int n = qobject_cast<const rnqnx::views::RnsvgElementItem*>(parent) ? 1 : 0;
  for (auto* child : parent->childItems()) n += countElements(child);
  return n;
}

std::string g_diagnostics;  // filled by the test so a timeout explains itself

std::string dumpTree(const QQuickItem* item, int depth = 0) {
  std::string out;
  if (const auto* e = qobject_cast<const rnqnx::views::RnsvgElementItem*>(item)) {
    const auto& p = e->props();
    out += std::string(depth * 2, ' ') + "kind=" + std::to_string(int(p.kind)) +
           " fill=" + std::to_string(int(p.fill.type)) + ":" + p.fill.color.name(QColor::HexArgb).toStdString() +
           " name=" + p.name.toStdString() + " w=" + std::to_string(p.width.value) +
           " propList=" + std::to_string(p.propList.size()) + "\n";
  } else if (const auto* r = qobject_cast<const rnqnx::views::RnsvgRootItem*>(item)) {
    out += std::string(depth * 2, ' ') + "root " + std::to_string(r->width()) + "x" + std::to_string(r->height()) +
           " paints=" + std::to_string(r->paintCount()) + "\n";
  }
  for (const auto* child : item->childItems()) out += dumpTree(child, depth + 1);
  return out;
}

void await(rnqnx::runtime::QtReactHost& host, const std::function<bool()>& ready,
           const char* what = "update") {
  QElapsedTimer timer;
  timer.start();
  do {
    host.runtime().drainMicrotasks();
    host.pumpEvents();
    if (ready()) return;
  } while (timer.elapsed() < 5000);
  std::string message = std::string("Timed out waiting for: ") + what + "\n" + g_diagnostics;
  for (const auto& error : host.javaScriptErrors()) message += "\n  JS error: " + error.message + "\n" + error.stack;
  for (const auto& failure : host.delegate().lastFailures()) message += "\n  mount failure: " + failure.detail.toStdString();
  throw std::runtime_error(message);
}

void evaluate(rnqnx::runtime::QtReactHost& host, const std::string& script) {
  if (!host.evaluate(script, "qt-rnsvg-fabric-test.js")) {
    throw std::runtime_error(host.lastError().toStdString());
  }
}

double number(rnqnx::runtime::QtReactHost& host, const char* name) {
  auto& runtime = host.runtime();
  return runtime.global().getProperty(runtime, name).asNumber();
}

bool near(const QColor& a, const QColor& b, int tolerance = 6) {
  return std::abs(a.red() - b.red()) <= tolerance && std::abs(a.green() - b.green()) <= tolerance &&
         std::abs(a.blue() - b.blue()) <= tolerance;
}

bool pixelIs(QQuickWindow& window, int x, int y, const QColor& color, int tolerance = 6) {
  const auto image = window.grabWindow();
  return !image.isNull() && near(image.pixelColor(x, y), color, tolerance);
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  try {
    QQuickWindow window;
    window.setColor(Qt::white);
    window.resize(220, 160);
    QQmlEngine engine;
    rnqnx::views::QtQuickItemFactory factory(engine);
    factory.registerType(QStringLiteral("View"),
        "import QtQuick\nItem { property color backgroundColor: 'transparent' }\n");
    rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 11);
    rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
    rnqnx::fabric::QtTouchDispatcher dispatcher(layer, *window.contentItem());
    adapter.setTouchDispatcher(&dispatcher);
    adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    rnqnx::runtime::QtReactHost host(adapter);
    host.delegate().setTouchDispatcher(&dispatcher);
    host.installTurboModuleBinding();
    host.installComponentProvider();
    host.installPlatformConstants();
    host.installTimers();
    for (const char* name : {"RNSVGSvgView", "RNSVGPath", "RNSVGGroup", "RNSVGLinearGradient", "RNSVGMask"}) {
      check(host.hasComponent(name), "react-native-svg descriptor missing from runtime");
    }
    check(!host.hasComponent("RNSVGPattern"), "unsupported Pattern must not be registered");
    std::ifstream file(RNQNX_RNSVG_BUNDLE_PATH);
    check(file.good(), "Metro react-native-svg fixture bundle is missing");
    const std::string bundle((std::istreambuf_iterator<char>(file)), {});
    evaluate(host, bundle);
    host.startSurface(11, "RnqnxRnsvgFixture", 220.0F, 160.0F);
    window.show();
    QObject input;
    input.installEventFilter(&dispatcher);

    await(host, [&] { return findRoot(window.contentItem()) != nullptr; }, "the <Svg> item to mount");
    check(host.delegate().lastFailures().empty(), "Fabric mount reported a failure");
    check(host.javaScriptErrors().empty(), "React reported a JavaScript error");
    QPointer<rnqnx::views::RnsvgRootItem> root = findRoot(window.contentItem());
    check(root->width() == 200 && root->height() == 150, "Yoga layout did not reach the <Svg> item");
    const int elements = countElements(root);
    check(elements >= 14, "Element tree was not mounted");

    // Gradient, group inheritance, clip, mask, use, path.
    g_diagnostics = dumpTree(window.contentItem());
    await(host, [&] {
      g_diagnostics = dumpTree(window.contentItem()) + "pixel(3,20)=" +
          window.grabWindow().pixelColor(3, 20).name(QColor::HexArgb).toStdString() + " pixel(20,70)=" +
          window.grabWindow().pixelColor(20, 70).name(QColor::HexArgb).toStdString() + "\n";
      return pixelIs(window, 3, 20, QColor(255, 0, 0), 24);  // a few percent into the gradient
    }, "gradient pixels");
    check(pixelIs(window, 97, 20, QColor(0, 0, 255), 24), "gradient end color wrong");
    check(pixelIs(window, 20, 70, QColor(0, 255, 0)), "<G fill> was not inherited");
    check(pixelIs(window, 70, 70, QColor(255, 0, 0)), "element fill did not override <G fill>");
    check(pixelIs(window, 150, 90, QColor(255, 0, 0)), "clipped rect missing inside the clip");
    check(pixelIs(window, 110, 10, QColor(255, 255, 255)), "clipPath did not clip");
    check(pixelIs(window, 50, 110, QColor(0, 0, 255)), "mask hid the visible half");
    check(pixelIs(window, 150, 110, QColor(255, 255, 255)), "mask did not hide the masked half");
    check(pixelIs(window, 110, 130, QColor(255, 128, 0)), "<Use> did not instantiate the definition");
    check(pixelIs(window, 40, 135, QColor(0, 255, 0)), "path fill missing");

    // Prop updates repaint the same retained elements; nothing is remounted.
    evaluate(host, "__rnsvgUpdate({pathFill:'#ffff00', gradientEnd:'#00ff00'});");
    await(host, [&] { return pixelIs(window, 40, 135, QColor(255, 255, 0)); });
    check(pixelIs(window, 97, 20, QColor(0, 255, 0), 24), "gradient stop update did not repaint");
    check(findRoot(window.contentItem()) == root.data(), "<Svg> was remounted by a prop update");
    check(countElements(root) == elements, "Element count changed on a prop update");

    // Conditional children come and go.
    check(pixelIs(window, 170, 30, QColor(128, 0, 128)), "circle missing");
    evaluate(host, "__rnsvgUpdate({showCircle:false});");
    await(host, [&] { return pixelIs(window, 170, 30, QColor(255, 255, 255)); });
    check(countElements(root) == elements - 1, "Removed element was not destroyed");

    // Per-element touch targets: the Path's own onPress fires, and the press on
    // empty space does not.
    const auto mouse = [&](bool down, QPointF point) {
      QMouseEvent event(down ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease, point, point,
          Qt::LeftButton, down ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
      QCoreApplication::sendEvent(&input, &event);
    };
    const auto settle = [&] {
      QElapsedTimer elapsed;
      elapsed.start();
      while (elapsed.elapsed() < 300) { host.runtime().drainMicrotasks(); host.pumpEvents(); }
    };
    mouse(true, {190, 60});  // outside every responsible element
    settle();
    mouse(false, {190, 60});
    settle();
    check(number(host, "__rnsvgPresses") == 0, "press on empty space reached a path");
    mouse(true, {40, 135});
    settle();
    mouse(false, {40, 135});
    await(host, [&] { return number(host, "__rnsvgPresses") == 1; });
    check(host.javaScriptErrors().empty(), "Touch handling reported a JavaScript error");

    // Resizing the Yoga box re-lays out the same retained tree.
    evaluate(host, "__rnsvgUpdate({width:100});");
    await(host, [&] { return root->width() == 100; });
    check(root.data() == findRoot(window.contentItem()), "resize remounted <Svg>");

    host.stopSurface(11);
    host.pumpEvents();
    check(host.javaScriptErrors().empty(), "react-native-svg lifecycle reported JavaScript errors");
    std::cout << "RNQNX_RNSVG_FABRIC_OK metro hermes fabric gradient clip mask use update remove touch resize\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
