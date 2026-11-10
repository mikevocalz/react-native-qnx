#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickWindow>
#include <cstdio>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
std::shared_ptr<int> run(const std::string& bundle, bool queueRemoteDelivery) {
  QQmlEngine engine;
  QQuickWindow window;
  rnqnx::views::QtQuickItemFactory factory(engine);
  rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 1);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  rnqnx::runtime::QtReactHost host(adapter);
  host.installTurboModuleBinding();
  host.setWorkletsWindow(&window);
  window.resize(64, 64);
  window.show();
  host.installComponentProvider();
  host.installPlatformConstants();
  host.installTimers();
  auto deliveries = std::make_shared<int>(0);
  auto& runtime = host.runtime();
  runtime.global().setProperty(runtime, "__recordDelivery",
      facebook::jsi::Function::createFromHostFunction(runtime,
        facebook::jsi::PropNameID::forAscii(runtime, "__recordDelivery"), 0,
        [deliveries](facebook::jsi::Runtime&, const facebook::jsi::Value&,
            const facebook::jsi::Value*, size_t) {
          ++*deliveries;
          return facebook::jsi::Value::undefined();
        }));
  require(host.evaluate(bundle, "worklets-probe.bundle.js"), host.lastError().toStdString());
  auto nativeVersion = runtime.global().getProperty(runtime, "_WORKLETS_VERSION_CPP");
  require(nativeVersion.isString() && nativeVersion.asString(runtime).utf8(runtime) == "0.13.1",
      "Unexpected native Worklets version");
  QElapsedTimer deadline;
  deadline.start();
  while (runtime.global().getProperty(runtime, "__workletResult").isUndefined() && deadline.elapsed() < 5000) {
    host.pumpEvents();
  }
  auto result = runtime.global().getProperty(runtime, "__workletResult");
  require(result.isNumber() && result.asNumber() == 42, "Actual Worklets UI-to-RN roundtrip failed: " + host.lastError().toStdString());
  auto uiKind = runtime.global().getProperty(runtime, "__workletRuntimeKind");
  require(uiKind.isNumber() && uiKind.asNumber() == 2, "Worklet ran on the wrong runtime");
  require(runtime.global().getProperty(runtime, "__rejectedBundleMode").getBool(), "Bundle Mode silently accepted");
  deadline.restart();
  while (!runtime.global().getProperty(runtime, "__frameTimestampSeen").isNumber() && deadline.elapsed() < 5000) host.pumpEvents();
  auto frameTimestamp = runtime.global().getProperty(runtime, "__frameTimestampSeen");
  require(frameTimestamp.isNumber() && frameTimestamp.asNumber() > 0, "No real Qt frame reached Worklets");
  require(host.evaluate("globalThis.__scheduleFailure();", "failed-worklet.js"), host.lastError().toStdString());
  deadline.restart();
  const auto reportedFailure = [&] {
    return std::any_of(host.javaScriptErrors().begin(), host.javaScriptErrors().end(), [](const auto& error) {
      return error.isFatal && error.message.find("RNQNX_EXPECTED_WORKLET_FAILURE") != std::string::npos;
    });
  };
  while (!reportedFailure() && deadline.elapsed() < 5000) {
    host.pumpEvents();
  }
  require(reportedFailure(), "UI exception was not reported through RN ExceptionsManager");
  require(host.evaluate("globalThis.__schedulePendingWorklet();", "pending-worklet.js"), host.lastError().toStdString());
  if (queueRemoteDelivery) {
    // Process the already-posted UI work. RN delivery it posts remains in the
    // owner queue for the next event turn, where host shutdown must cancel it.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  }
  require(host.evaluate("Object.defineProperty(globalThis, '__workletsModuleProxy', {writable: false, configurable: false});", "freeze-proxy.js"), host.lastError().toStdString());
  // Leave a queued worklet at host destruction. The next host/event pump must
  // not call into the destroyed RN runtime or preserve its global state.
  require(*deliveries == 1, "Remote delivery was not left pending");
  return deliveries;
}
}
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    require(argc == 2, "Pass production Worklets probe bundle");
    QFile file(QString::fromLocal8Bit(argv[1]));
    require(file.open(QIODevice::ReadOnly), "Cannot read Worklets bundle");
    const auto bundle = file.readAll().toStdString();
    auto first = run(bundle, false);
    QCoreApplication::processEvents();
    require(*first == 1, "Queued UI work delivered after host destruction");
    auto second = run(bundle, true);
    QCoreApplication::processEvents();
    require(*second == 1, "Queued RN work delivered after host destruction");
    std::puts("RNQNX_WORKLETS_RUNTIME_OK real-upstream-roundtrip reload queued-teardown (Linux host only)");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
