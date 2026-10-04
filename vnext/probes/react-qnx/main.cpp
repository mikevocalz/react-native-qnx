// One executable source for host readiness and the later real QNX run.
// Only a loaded Metro bundle may create/update the items asserted here.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <QColor>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QThread>

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
constexpr facebook::react::SurfaceId kSurfaceId = 1;
constexpr const char* kViewSource = R"QML(import QtQuick
Rectangle {
  visible: true
  property color backgroundColor: "transparent"
  color: backgroundColor
  property real borderWidth: 0
  property color borderColor: "transparent"
  property bool usePerCornerRadius: false
}
)QML";

void require(bool condition, const char* reason) {
  if (!condition) {
    throw std::runtime_error(reason);
  }
}

void near(qreal actual, qreal expected, const char* reason) {
  require(std::isfinite(actual) && std::abs(actual - expected) < 0.01, reason);
}

void assertHealthy(rnqnx::runtime::QtReactHost& host) {
  if (!host.delegate().lastFailures().empty()) {
    const auto& failure = host.delegate().lastFailures().front();
    throw std::runtime_error("fabric-mount-failure tag=" + std::to_string(failure.tag) +
                             ": " + failure.detail.toStdString());
  }
  if (!host.javaScriptErrors().empty()) {
    throw std::runtime_error("javascript-error: " + host.javaScriptErrors().back().message);
  }
  require(host.lastError().isEmpty(), "host-error");
}

template <typename Predicate>
void waitFor(rnqnx::runtime::QtReactHost& host, Predicate ready, const char* reason) {
  QElapsedTimer deadline;
  deadline.start();
  do {
    host.runtime().drainMicrotasks();
    host.pumpEvents();
    assertHealthy(host);
    if (ready()) {
      return;
    }
    QThread::msleep(1);
  } while (deadline.elapsed() < 5000);
  throw std::runtime_error(reason);
}

void assertGeometry(QQuickItem& root, bool updated) {
  require(root.childItems().size() == 1, "root-child-count");
  const auto* outer = root.childItems().at(0);
  near(outer->x(), 0, "outer-x");
  near(outer->y(), 0, "outer-y");
  near(outer->width(), 160, "outer-width");
  near(outer->height(), 100, "outer-height");
  require(outer->property("backgroundColor").value<QColor>() == QColor("#204080"),
          "outer-color");
  require(outer->childItems().size() == 1, "inner-child-count");
  const auto* inner = outer->childItems().at(0);
  near(inner->x(), updated ? 40 : 12, "inner-x");
  near(inner->y(), 16, "inner-y");
  near(inner->width(), updated ? 72 : 48, "inner-width");
  near(inner->height(), 24, "inner-height");
  require(inner->property("backgroundColor").value<QColor>() ==
              QColor(updated ? "#00cc66" : "#ff8800"),
          "inner-color");
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QCommandLineParser parser;
  parser.setApplicationDescription("Real bundled React/Fabric geometry probe; no pixel or device claim.");
  parser.addHelpOption();
  const QCommandLineOption bundleOption("bundle", "Metro probe bundle path (required on target).", "path");
  parser.addOption(bundleOption);
  parser.process(app);

  try {
    // A compiled-in host path is convenient locally, but never points the QNX
    // binary at the build host's filesystem. Target deployment must be explicit.
    QString bundlePath = parser.value(bundleOption);
#ifdef RNQNX_REACT_PROBE_BUNDLE_PATH
    if (bundlePath.isEmpty()) {
      bundlePath = QStringLiteral(RNQNX_REACT_PROBE_BUNDLE_PATH);
    }
#endif
    require(!bundlePath.isEmpty(), "bundle-required");
    QFile bundleFile(bundlePath);
    require(bundleFile.open(QIODevice::ReadOnly), "bundle-open");
    const auto bundle = bundleFile.readAll();
    require(bundleFile.error() == QFile::NoError && !bundle.isEmpty(), "bundle-read");

    QQmlEngine engine;
    QQuickItem root;
    root.setWidth(240);
    root.setHeight(160);
    rnqnx::views::QtQuickItemFactory factory(engine);
    factory.registerType(QStringLiteral("View"), QByteArray(kViewSource));
    rnqnx::fabric::QtQuickMountingLayer layer(factory, root, kSurfaceId);
    rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
    adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    rnqnx::runtime::QtReactHost host(adapter);
    host.installTurboModuleBinding();
    host.installComponentProvider();
    host.installPlatformConstants();
    host.installTimers();
    require(host.hasFabricUIManager(), "fabric-ui-manager-missing");
    if (!host.evaluate(bundle.toStdString(), bundlePath.toStdString())) {
      throw std::runtime_error("bundle-evaluate: " + host.lastError().toStdString());
    }

    // RN's SurfaceHandler/UIManager startup is noexcept. Reject an unrelated
    // valid script before that boundary instead of terminating the process.
    require(host.evaluate(R"JS((function () {
      const registry = globalThis.RN$AppRegistry;
      if (!registry || typeof registry.runApplication !== 'function' ||
          typeof registry.getRunnable !== 'function' ||
          typeof registry.getRunnable('RNQnxReactRenderProbe') !== 'function') {
        throw new Error('probe-component-not-registered');
      }
    })();)JS", "probe-registration.js"), "bundle-registration");

    host.startSurface(kSurfaceId, "RNQnxReactRenderProbe", 240.0F, 160.0F);
    waitFor(host, [&] {
      return host.delegate().mountedTransactionCount() > 0 && !root.childItems().empty();
    }, "initial-mount-timeout");
    assertGeometry(root, false);
    require(layer.mountedCount() == 3, "initial-mounted-count");
    const auto initialCleanup = host.runtime().global().getProperty(
        host.runtime(), "__rnqnxProbeUnmounted");
    require(initialCleanup.isBool() && !initialCleanup.getBool(), "react-mount-effect");

    const int initialTransactions = host.delegate().mountedTransactionCount();
    require(host.evaluate("globalThis.__rnqnxUpdateProbe();", "probe-update.js"), "update-evaluate");
    waitFor(host, [&] {
      return host.delegate().mountedTransactionCount() > initialTransactions;
    }, "update-mount-timeout");
    assertGeometry(root, true);
    require(layer.mountedCount() == 3, "updated-mounted-count");

    const int updatedTransactions = host.delegate().mountedTransactionCount();
    host.stopSurface(kSurfaceId);
    waitFor(host, [&] {
      const auto cleanup = host.runtime().global().getProperty(
          host.runtime(), "__rnqnxProbeUnmounted");
      return host.delegate().mountedTransactionCount() > updatedTransactions &&
          root.childItems().empty() && layer.mountedCount() == 1 &&
          cleanup.isBool() && cleanup.getBool();
    }, "unmount-timeout");
#ifdef __QNXNTO__
    constexpr const char* platform = "qnx";
#else
    constexpr const char* platform = "host";
#endif
    std::cout << "RNQNX_REACT_RENDER_OK platform=" << platform
              << " bundled-react initial=160x100/48x24 update=72x24 unmount=empty geometry-only transactions="
              << host.delegate().mountedTransactionCount() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_REACT_RENDER_ERROR " << error.what() << '\n';
    return 1;
  }
}
