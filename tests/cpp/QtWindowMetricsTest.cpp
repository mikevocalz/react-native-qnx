// Real Qt window/client observation and the production RN Dimensions/hook path.
// This is a Linux offscreen scale fixture, not physical QNX display evidence.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/runtime/QtWindowMetricsObserver.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/views/QtTextInputQml.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QFont>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>

#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void near(double value, double expected, const char* message) {
  if (!std::isfinite(value) || std::abs(value - expected) > 0.01) {
    throw std::runtime_error(std::string(message) + ": " + std::to_string(value) +
        " != " + std::to_string(expected));
  }
}
void settle(rnqnx::runtime::QtReactHost& host, const std::function<bool()>& ready) {
  QElapsedTimer deadline;
  deadline.start();
  while (!ready() && deadline.elapsed() < 3000) host.pumpEvents();
  require(ready(), "Runtime/window metrics did not settle");
}
double number(rnqnx::runtime::QtReactHost& host, const char* group, const char* property) {
  auto& runtime = host.runtime();
  const auto value = runtime.global().getProperty(runtime, "__windowMetricsProbe");
  if (!value.isObject()) return -1;
  auto object = value.asObject(runtime);
  if (group) {
    const auto nested = object.getProperty(runtime, group);
    if (!nested.isObject()) return -1;
    object = nested.asObject(runtime);
  }
  const auto result = object.getProperty(runtime, property);
  return result.isNumber() ? result.getNumber() : -1;
}
void evaluate(rnqnx::runtime::QtReactHost& host, const char* script) {
  if (!host.evaluate(script, "window-metrics-test.js")) {
    throw std::runtime_error(host.lastError().toStdString());
  }
}
QQuickItem* findNativeId(QQuickItem& root, const char* nativeId) {
  if (root.property("nativeID").toString() == QString::fromUtf8(nativeId)) return &root;
  for (auto* child : root.childItems()) {
    if (auto* result = findNativeId(*child, nativeId)) return result;
  }
  return nullptr;
}

void observerLifetime() {
  QQuickWindow window;
  window.resize(800, 600);
  QQuickItem client(window.contentItem());
  client.setSize(QSizeF(700, 450));
  int deliveries = 0;
  {
    rnqnx::runtime::QtWindowMetricsObserver observer(window, client,
        [&](const auto&) { ++deliveries; });
    client.setWidth(680);
    client.setHeight(430);
    // Destroy with a pending coalesced event; neither signal nor timer survives.
  }
  QCoreApplication::processEvents();
  require(deliveries == 0, "Destroyed observer delivered pending geometry");
  client.setWidth(660);
  QCoreApplication::processEvents();
  require(deliveries == 0, "Destroyed observer remained connected");

  auto transient = std::make_unique<QQuickItem>(window.contentItem());
  transient->setSize(QSizeF(100, 100));
  rnqnx::runtime::QtWindowMetricsObserver observer(window, *transient,
      [&](const auto&) { ++deliveries; });
  transient->setWidth(120);
  transient.reset();
  QCoreApplication::processEvents();
  require(deliveries == 0, "Deleted client delivered stale geometry");
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try {
    observerLifetime();
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(800, 600);
    QQuickItem client(window.contentItem());
    client.setPosition(QPointF(20, 60));
    client.setSize(QSizeF(720, 480));
    window.show();
    QCoreApplication::processEvents();
    const double scale = window.devicePixelRatio();
    near(scale, qEnvironmentVariable("RNQNX_EXPECTED_SCALE", "1").toDouble(),
        "Qt fixture did not select expected scale");

    rnqnx::views::QtQuickItemFactory factory(engine);
    factory.registerType(QStringLiteral("View"), QByteArray(
        "import QtQuick\nItem { property color backgroundColor: 'transparent' }\n"));
    factory.registerType(QStringLiteral("Text"), QByteArray("import QtQuick\nText { wrapMode: Text.Wrap }\n"));
    factory.registerType(QStringLiteral("TextInput"), QByteArray(rnqnx::views::textInputQml()));
    constexpr facebook::react::SurfaceId surfaceId = 31;
    rnqnx::fabric::QtQuickMountingLayer layer(factory, client, surfaceId);
    rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
    adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    adapter.mapComponent(QStringLiteral("Paragraph"), QStringLiteral("Text"));
    adapter.mapComponent(QStringLiteral("TextInput"), QStringLiteral("TextInput"));
    auto host = std::make_unique<rnqnx::runtime::QtReactHost>(adapter);
    require(host->observeWindowMetrics(surfaceId, window, client), "Failed initial window observation");
    require(!host->observeWindowMetrics(surfaceId, window, client), "Accepted a second window binding");
    host->installTurboModuleBinding();
    host->installComponentProvider();
    host->installPlatformConstants();
    host->installTimers();
    std::ifstream input(RNQNX_WINDOW_METRICS_BUNDLE_PATH);
    require(static_cast<bool>(input), "Metrics bundle is missing");
    std::stringstream buffer;
    buffer << input.rdbuf();
    require(host->evaluate(buffer.str(), RNQNX_WINDOW_METRICS_BUNDLE_PATH), "Metrics bundle failed evaluation");
    near(number(*host, "initial", "width"), 720, "Initial width was screen or scaled twice");
    near(number(*host, "initial", "height"), 480, "Initial height included native chrome");
    near(number(*host, "initial", "scale"), scale, "Initial DPR differed from the window");
    near(number(*host, "screen", "width"), window.screen()->geometry().width(), "Screen metrics were client bounds");
    host->startSurface(surfaceId, "RNQnxWindowMetrics", 720, 480);
    settle(*host, [&] { return number(*host, "first", "width") == 720 && number(*host, "second", "width") == 720; });
    require(!client.childItems().empty(), "React root did not mount");
    auto* root = client.childItems().front();
    near(root->width(), 720, "Fabric root did not use client width");
    near(root->height(), 480, "Fabric root did not use client height");
    near(number(*host, "first", "pixelRatio"), scale, "PixelRatio diverged from native DPR");
    QPointer<QQuickItem> text = findNativeId(client, "metrics-text");
    QPointer<QQuickItem> emptyInput = findNativeId(client, "metrics-empty-input");
    require(text && emptyInput, "Typography consumers did not mount");
    require(text->property("font").value<QFont>().pixelSize() == 16 &&
        emptyInput->property("font").value<QFont>().pixelSize() == 16,
        "Initial logical font size was scaled by DPR or lost");

    require(!host->resizeSurface(surfaceId, 300, 200),
        "Manual resize overrode an observed native client");
    require(host->lastError().contains(QStringLiteral("resize the client item or window")),
        "Observed-surface resize failure did not explain the geometry owner");
    near(root->width(), 720, "Rejected manual resize changed Yoga width");
    near(number(*host, "first", "width"), 720, "Rejected manual resize changed Dimensions");

    evaluate(*host, "__bumpMetricsState(); __removeFirstMetricsListener();");
    settle(*host, [&] { return number(*host, "first", "count") == 1; });
    const double firstEvents = number(*host, nullptr, "firstEvents");
    const double secondEvents = number(*host, nullptr, "secondEvents");
    client.setSize(QSizeF(610, 410));
    client.setSize(QSizeF(640.5, 420.5));
    client.setSize(QSizeF(650.5, 430.5));
    settle(*host, [&] { return number(*host, "first", "width") == 650.5 && number(*host, "second", "height") == 430.5; });
    near(number(*host, nullptr, "firstEvents"), firstEvents, "Removed listener still received updates");
    near(number(*host, nullptr, "secondEvents"), secondEvents + 1, "Geometry burst did not coalesce coherently");
    near(number(*host, "first", "count"), 1, "Resize reset React state");
    require(client.childItems().front() == root, "Resize replaced the mounted React root");
    // Yoga pixel rounding can move a fractional edge by <= half a device pixel.
    require(std::abs(root->width() - 650.5) <= 0.5 / scale + 0.01, "Native root disagreed with logical width");
    near(number(*host, "screen", "width"), window.screen()->geometry().width(), "Resize changed the full screen width");
    near(number(*host, "first", "fontScale"), 1, "Density changed baseline text scale");

    require(host->setFontScale(1.5), "Valid font-scale update failed");
    settle(*host, [&] { return number(*host, "first", "fontScale") == 1.5 && number(*host, "second", "fontRatio") == 1.5; });
    settle(*host, [&] {
      return text && emptyInput && text->property("font").value<QFont>().pixelSize() == 23 &&
          emptyInput->property("font").value<QFont>().pixelSize() == 23;
    });
    require(findNativeId(client, "metrics-text") == text &&
        findNativeId(client, "metrics-empty-input") == emptyInput,
        "Font scale replaced the mounted text consumers");
    require(emptyInput->property("text").toString().isEmpty(), "Empty input fixture acquired text");
    near(number(*host, "first", "pixelRatio"), scale, "Text scale changed DPR");
    require(!host->setFontScale(0), "Accepted zero font scale");
    require(!host->setFontScale(std::numeric_limits<double>::quiet_NaN()), "Accepted NaN font scale");
    require(!host->setFontScale((std::numeric_limits<double>::max)()), "Accepted font scale that overflows layout floats");
    near(number(*host, "first", "fontScale"), 1.5, "Invalid font scale changed the snapshot");
    // A DPR notification with unchanged Qt observations is deduplicated.
    const auto before = number(*host, nullptr, "secondEvents");
    QEvent densityEvent(QEvent::DevicePixelRatioChange);
    QCoreApplication::sendEvent(&window, &densityEvent);
    for (int turn = 0; turn < 20; ++turn) {
      host->runtime().drainMicrotasks();
      host->pumpEvents();
    }
    near(number(*host, nullptr, "secondEvents"), before, "Duplicate DPR event emitted metrics");
    evaluate(*host, "__removeSecondMetricsListener();");
    require(host->javaScriptErrors().empty(), "Metrics consumer reported JavaScript errors");
    require(host->delegate().lastFailures().empty(), "Metrics consumer had native mount failures");
    require(host->setFontScale(1.75), "Could not queue final metrics before teardown");
    // The DeviceInfo event is now queued on RN's invoker, and geometry also
    // has a pending native timer. Both must become inert before Hermes closes.
    client.setWidth(620); // Pending native callback must not survive runtime teardown.
    host.reset();
    QCoreApplication::processEvents();
    client.setWidth(600);
    QCoreApplication::processEvents();
    std::cout << "RNQNX_WINDOW_METRICS_OK scale=" << scale
              << " initial client screen fractional resize two-consumers state cleanup font-scale native-text empty-input teardown" << std::endl;
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_WINDOW_METRICS_FAIL " << error.what() << std::endl;
    return 1;
  }
}
