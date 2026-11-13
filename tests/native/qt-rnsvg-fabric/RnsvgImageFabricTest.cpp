// react-native-svg <Image> through Metro, Hermes, Fabric and the Qt mounting
// layer: a data URI, a bundled asset, the onLoad event, a reload on href
// change, and a source that fails. Pixels come from the real window.
//
// Needs the Hermes host build, so it runs in the Fabric test job and not in
// the native renderer tests.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <RnsvgItems.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QUrl>

#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

bool hasRoot(QQuickItem* parent) {
  if (qobject_cast<rnqnx::views::RnsvgRootItem*>(parent)) return true;
  for (auto* child : parent->childItems()) {
    if (hasRoot(child)) return true;
  }
  return false;
}

void await(rnqnx::runtime::QtReactHost& host, const std::function<bool()>& ready, const char* what) {
  QElapsedTimer timer;
  timer.start();
  do {
    host.runtime().drainMicrotasks();
    host.pumpEvents();
    if (ready()) return;
  } while (timer.elapsed() < 5000);
  std::string message = std::string("Timed out waiting for: ") + what;
  for (const auto& error : host.javaScriptErrors()) message += "\n  JS error: " + error.message + "\n" + error.stack;
  for (const auto& failure : host.delegate().lastFailures()) message += "\n  mount failure: " + failure.detail.toStdString();
  throw std::runtime_error(message);
}

void evaluate(rnqnx::runtime::QtReactHost& host, const std::string& script, const std::string& source) {
  if (!host.evaluate(script, source)) throw std::runtime_error(host.lastError().toStdString());
}

facebook::jsi::Value global(rnqnx::runtime::QtReactHost& host, const char* name) {
  auto& runtime = host.runtime();
  return runtime.global().getProperty(runtime, name);
}

std::string text(rnqnx::runtime::QtReactHost& host, const char* name) {
  auto& runtime = host.runtime();
  return global(host, name).asString(runtime).utf8(runtime);
}

// `__rnsvgImageLoads.<name>` and `__rnsvgImageLast.<name>`.
double loads(rnqnx::runtime::QtReactHost& host, const char* name) {
  auto& runtime = host.runtime();
  return global(host, "__rnsvgImageLoads").asObject(runtime).getProperty(runtime, name).asNumber();
}
std::string lastPayload(rnqnx::runtime::QtReactHost& host, const char* name) {
  auto& runtime = host.runtime();
  return global(host, "__rnsvgImageLast").asObject(runtime).getProperty(runtime, name).asString(runtime).utf8(runtime);
}

bool pixelIs(QQuickWindow& window, int x, int y, const QColor& color, int tolerance = 8) {
  const auto image = window.grabWindow();
  if (image.isNull()) return false;
  const QColor got = image.pixelColor(x, y);
  return std::abs(got.red() - color.red()) <= tolerance && std::abs(got.green() - color.green()) <= tolerance &&
         std::abs(got.blue() - color.blue()) <= tolerance;
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
    rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 12);
    rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
    adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    rnqnx::runtime::QtReactHost host(adapter);
    host.installTurboModuleBinding();
    host.installComponentProvider();
    host.installPlatformConstants();
    host.installTimers();

    const std::string bundlePath = RNQNX_RNSVG_IMAGE_BUNDLE_PATH;
    std::ifstream file(bundlePath);
    check(file.good(), "Metro react-native-svg Image fixture bundle is missing");
    const std::string bundle((std::istreambuf_iterator<char>(file)), {});
    // A file:// source URL makes Image.resolveAssetSource return a file://
    // URL beside the bundle, the way a bundle read from disk resolves it.
    evaluate(host, bundle, "file://" + bundlePath);
    check(host.javaScriptErrors().empty(), "The fixture bundle reported a JavaScript error");

    // `metro build` does not write assets. Put the PNG where React Native says
    // it will look, then check that place is beside the bundle.
    const QString assetUri = QString::fromStdString(text(host, "__rnsvgImageAssetUri"));
    const QString bundleDir = QFileInfo(QString::fromStdString(bundlePath)).absolutePath();
    check(assetUri.startsWith(QStringLiteral("file://") + bundleDir + QStringLiteral("/assets/")),
          "A bundled asset did not resolve to a file URL beside the bundle");
    const QString assetFile = QUrl(assetUri).toLocalFile();
    check(QDir().mkpath(QFileInfo(assetFile).absolutePath()), "Could not create the asset directory");
    QFile::remove(assetFile);
    check(QFile::copy(QStringLiteral(RNQNX_RNSVG_IMAGE_ASSET_PATH), assetFile), "Could not copy the asset PNG");

    host.startSurface(12, "RnqnxRnsvgImageFixture", 220.0F, 160.0F);
    window.show();
    await(host, [&] { return hasRoot(window.contentItem()); }, "the <Svg> item to mount");
    check(host.delegate().lastFailures().empty(), "Fabric mount reported a failure");

    // The data URI image: red, green / blue, yellow, stretched over 50x50.
    await(host, [&] { return pixelIs(window, 10, 10, QColor(255, 0, 0)); }, "the data URI image to paint");
    check(pixelIs(window, 40, 10, QColor(0, 255, 0)), "data URI image: top right is wrong");
    check(pixelIs(window, 10, 40, QColor(0, 0, 255)), "data URI image: bottom left is wrong");
    check(pixelIs(window, 40, 40, QColor(255, 255, 0)), "data URI image: bottom right is wrong");

    // The bundled asset: magenta, cyan / orange, violet.
    await(host, [&] { return pixelIs(window, 110, 10, QColor(255, 0, 255)); }, "the bundled asset to paint");
    check(pixelIs(window, 140, 10, QColor(0, 255, 255)), "bundled asset: top right is wrong");
    check(pixelIs(window, 110, 40, QColor(255, 128, 0)), "bundled asset: bottom left is wrong");
    check(pixelIs(window, 140, 40, QColor(128, 0, 255)), "bundled asset: bottom right is wrong");

    // onLoad: once each, with the decoded size and the URI JavaScript supplied.
    await(host, [&] { return loads(host, "data") == 1 && loads(host, "asset") == 1; }, "both onLoad events");
    const std::string data = lastPayload(host, "data");
    check(data.find("\"width\":2") != std::string::npos && data.find("\"height\":2") != std::string::npos,
          "data URI onLoad carries the wrong size");
    check(data.find(text(host, "__rnsvgImageFirst")) != std::string::npos, "data URI onLoad carries the wrong uri");
    const std::string asset = lastPayload(host, "asset");
    check(asset.find("\"width\":4") != std::string::npos && asset.find("\"height\":4") != std::string::npos,
          "asset onLoad carries the wrong size");
    check(asset.find(assetUri.toStdString()) != std::string::npos, "asset onLoad carries the wrong uri");

    // A source that is not an image draws nothing and reports nothing.
    check(pixelIs(window, 25, 75, QColor(255, 255, 255)), "the broken image painted something");
    check(loads(host, "broken") == 0, "the broken image reported onLoad");

    // A new href reloads that element and only that element.
    evaluate(host, "__rnsvgImageUpdate({dataHref:'second'});", "qt-rnsvg-image-fabric-test.js");
    await(host, [&] { return pixelIs(window, 10, 10, QColor(0, 0, 0)); }, "the new data URI image to paint");
    check(pixelIs(window, 40, 10, QColor(255, 255, 255)), "second data URI image: top right is wrong");
    await(host, [&] { return loads(host, "data") == 2; }, "the second onLoad for the data image");
    check(loads(host, "asset") == 1, "an unchanged href was loaded again");
    check(pixelIs(window, 110, 10, QColor(255, 0, 255)), "the asset image changed with its sibling");

    // A re-render that keeps every href reloads nothing.
    evaluate(host, "__rnsvgImageUpdate({width:200});", "qt-rnsvg-image-fabric-test.js");
    for (int i = 0; i < 20; ++i) {
      host.runtime().drainMicrotasks();
      host.pumpEvents();
    }
    check(loads(host, "data") == 2 && loads(host, "asset") == 1, "a re-render with the same hrefs reloaded an image");

    host.stopSurface(12);
    host.pumpEvents();
    check(host.javaScriptErrors().empty(), "the Image lifecycle reported JavaScript errors");
    std::cout << "RNQNX_RNSVG_IMAGE_FABRIC_OK data-uri bundled-asset onLoad reload broken-source\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
