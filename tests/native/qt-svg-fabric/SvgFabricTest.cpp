#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <QtSvgItem.h>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
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

rnqnx::views::QtSvgItem* findSvg(QQuickItem* parent) {
  if (auto* item = qobject_cast<rnqnx::views::QtSvgItem*>(parent)) return item;
  for (auto* child : parent->childItems()) {
    if (auto* item = findSvg(child)) return item;
  }
  return nullptr;
}

void await(rnqnx::runtime::QtReactHost& host, const std::function<bool()>& ready) {
  QElapsedTimer timer;
  timer.start();
  do {
    host.runtime().drainMicrotasks();
    host.pumpEvents();
    if (ready()) return;
  } while (timer.elapsed() < 3000);
  throw std::runtime_error("Timed out waiting for the real React/Fabric update");
}

void evaluate(rnqnx::runtime::QtReactHost& host, const std::string& script) {
  if (!host.evaluate(script, "qt-svg-fabric-test.js")) {
    throw std::runtime_error(host.lastError().toStdString());
  }
}

int eventCount(rnqnx::runtime::QtReactHost& host) {
  auto& runtime = host.runtime();
  return static_cast<int>(runtime.global().getPropertyAsObject(runtime, "__qtSvgEvents")
      .asArray(runtime).size(runtime));
}

bool pixelIs(QQuickWindow& window, int x, int y, const QColor& color) {
  const auto image = window.grabWindow();
  return !image.isNull() && image.pixelColor(x, y) == color;
}
}

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  try {
    QQuickWindow window;
    window.setColor(Qt::white);
    window.resize(220, 150);
    QQmlEngine engine;
    rnqnx::views::QtQuickItemFactory factory(engine);
    factory.registerType(QStringLiteral("View"),
        "import QtQuick\nItem { property color backgroundColor: 'transparent' }\n");
    rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 11);
    rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
    adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    rnqnx::runtime::QtReactHost host(adapter);
    host.installTurboModuleBinding();
    host.installComponentProvider();
    host.installPlatformConstants();
    host.installTimers();
    check(host.hasComponent("QnxSvgDocument"), "SVG descriptor missing from runtime");
    std::ifstream file(RNQNX_QT_SVG_BUNDLE_PATH);
    check(file.good(), "Metro SVG fixture bundle is missing");
    const std::string bundle((std::istreambuf_iterator<char>(file)), {});
    evaluate(host, bundle);
    evaluate(host, "if (__qtSvgRejectedProps !== 3) throw new Error('Invalid component props were accepted');");
    host.startSurface(11, "RnqnxQtSvgFixture", 220.0F, 150.0F);
    window.show();
    await(host, [&] { return findSvg(window.contentItem()) && eventCount(host) == 1; });
    check(host.delegate().lastFailures().empty(), "Fabric mount reported a failure");
    check(host.javaScriptErrors().empty(), "React reported a JavaScript error");
    QPointer<rnqnx::views::QtSvgItem> svg = findSvg(window.contentItem());
    check(svg->width() == 100 && svg->height() == 60, "Yoga layout did not reach SVG item");
    await(host, [&] { return pixelIs(window, 54, 35, Qt::red); });
    evaluate(host, "if (__qtSvgEvents[0].width !== 100 || __qtSvgEvents[0].height !== 50) throw new Error('Wrong intrinsic size');");

    evaluate(host, "__qtSvgUpdate({width:160,height:80,resizeMode:'stretch'});");
    await(host, [&] { return svg->width() == 160 && svg->height() == 80; });
    check(eventCount(host) == 1, "Layout-only update decoded the same document again");
    await(host, [&] { return pixelIs(window, 150, 75, Qt::red); });

    evaluate(host, R"JS(__qtSvgUpdate({xml:'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><rect width="10" height="10" fill="#0000ff"/></svg>'});)JS");
    await(host, [&] { return eventCount(host) == 2; });
    await(host, [&] { return pixelIs(window, 54, 35, Qt::blue); });
    evaluate(host, R"JS(__qtSvgUpdate({xml:'<svg xmlns="http://www.w3.org/2000/svg"><script/></svg>'});)JS");
    await(host, [&] { return eventCount(host) == 3; });
    check(svg->documentState() == rnqnx::views::SvgDocumentState::Error, "Invalid source was retained");
    evaluate(host, "if (__qtSvgEvents[2].kind !== 'error' || !__qtSvgEvents[2].message) throw new Error('Missing native error event');");
    await(host, [&] { return pixelIs(window, 54, 35, Qt::white); });

    evaluate(host, "__qtSvgUpdate({xml:' '.repeat(65537)});");
    await(host, [&] { return eventCount(host) == 4; });
    evaluate(host, "if (__qtSvgEvents[3].kind !== 'error') throw new Error('Oversized XML was accepted');");

    evaluate(host, "__qtSvgUpdate({xml:''});");
    await(host, [&] { return svg->documentState() == rnqnx::views::SvgDocumentState::Empty; });
    check(eventCount(host) == 4, "Clearing a document emitted an error or load");
    evaluate(host, "__qtSvgUpdate({visible:false});");
    await(host, [&] { return svg.isNull(); });
    host.stopSurface(11);
    host.pumpEvents();
    check(host.javaScriptErrors().empty(), "SVG lifecycle reported JavaScript errors");
    std::cout << "RNQNX_QT_SVG_FABRIC_OK metro hermes yoga pixels events replace clear unmount\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
