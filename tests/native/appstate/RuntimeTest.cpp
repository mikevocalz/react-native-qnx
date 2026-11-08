#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#if RNQNX_ENABLE_APP_STATE
#include "react-native-qnx/runtime/QnxAppStateModule.h"
#include <ReactCommon/CallInvoker.h>
#endif

#include <QFile>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using rnqnx::runtime::QtReactHost;
namespace jsi = facebook::jsi;

void require(bool success, const std::string& message) {
  if (!success) throw std::runtime_error(message);
}

struct Fixture {
  QQmlEngine engine;
  rnqnx::views::QtQuickItemFactory factory{engine};
  QQuickItem root;
  rnqnx::fabric::QtQuickMountingLayer layer{factory, root, 1};
  rnqnx::fabric::ShadowViewMutationAdapter adapter{layer};
  std::unique_ptr<QtReactHost> host{std::make_unique<QtReactHost>(adapter)};

  Fixture() {
    host->installTurboModuleBinding();
    host->installComponentProvider();
    host->installPlatformConstants();
    host->installTimers();
  }
  void evaluate(const std::string& source) {
    require(host->evaluate(source, "appstate-assert.js"), host->lastError().toStdString());
  }
  void assertJS(const std::string& expression) {
    evaluate("if (!(" + expression + ")) throw new Error(" +
        std::string("'AppState assertion failed'") + ");");
  }
};

#if RNQNX_ENABLE_APP_STATE
void notify(Qt::ApplicationState state) {
  // Real Qt signal delivery; synthetic input, NOT OS suspend/freeze evidence.
  require(QMetaObject::invokeMethod(qGuiApp, "applicationStateChanged", Qt::DirectConnection,
      Q_ARG(Qt::ApplicationState, state)), "Missing Qt application-state signal");
}

class DeferredInvoker final : public facebook::react::CallInvoker {
 public:
  void invokeAsync(facebook::react::CallFunc&& function) noexcept override {
    tasks.push_back(std::move(function));
  }
  void invokeSync(facebook::react::CallFunc&&) override {
    throw std::logic_error("AppState must not need synchronous cross-context invocation");
  }
  void drain(jsi::Runtime& runtime) {
    auto pending = std::move(tasks);
    tasks.clear();
    for (auto& task : pending) task(runtime);
  }
  std::vector<facebook::react::CallFunc> tasks;
};

void pendingDeliveriesCannotOutliveObservation() {
  Fixture fixture;
  fixture.evaluate("globalThis.lateEvents = 0; globalThis.__rctDeviceEventEmitter = {emit() { lateEvents++; }};");
  auto invoker = std::make_shared<DeferredInvoker>();
  auto module = std::make_shared<rnqnx::runtime::QnxAppStateModule>(invoker);
  notify(Qt::ApplicationActive);
  notify(Qt::ApplicationHidden);
  require(!invoker->tasks.empty(), "Negative lifetime control must contain accepted work");
  module->invalidate();
  module->invalidate();
  invoker->drain(fixture.host->runtime());
  fixture.assertJS("lateEvents === 0");
  module.reset();
  notify(Qt::ApplicationActive);
  require(invoker->tasks.empty(), "Destroyed observer still produced work");
}
#endif

void publicContract(const std::string& bundle, const std::string& platform) {
  Fixture fixture;
  require(fixture.host->evaluate(bundle, "appstate.bundle.js"), fixture.host->lastError().toStdString());
  fixture.evaluate("globalThis.p = globalThis.__rnqnxAppStateProbe;");
  fixture.assertJS("p.platform === '" + platform + "'");
#if RNQNX_ENABLE_APP_STATE
  fixture.assertJS("p.available === true && ['active','inactive','background','unknown'].includes(p.current)");
  notify(Qt::ApplicationInactive);
  fixture.host->pumpEvents();
  fixture.assertJS("p.current === 'inactive'");
  fixture.evaluate("p.clear();");
  notify(Qt::ApplicationHidden);
  notify(Qt::ApplicationSuspended);
  notify(Qt::ApplicationActive);
  fixture.host->pumpEvents();
  fixture.assertJS("JSON.stringify(p.first) === '[\"background\",\"active\"]'");
  fixture.assertJS("JSON.stringify(p.second) === JSON.stringify(p.first) && p.current === 'active'");
  fixture.evaluate("p.removeFirst(); p.removeFirst(); p.clear();");
  notify(Qt::ApplicationInactive);
  fixture.host->pumpEvents();
  fixture.assertJS("p.first.length === 0 && JSON.stringify(p.second) === '[\"inactive\"]'");
  fixture.evaluate("p.removeAll(); p.clear();");
  notify(Qt::ApplicationActive);
  fixture.host->pumpEvents();
  fixture.assertJS("p.first.length === 0 && p.second.length === 0 && p.current === 'active'");
  notify(static_cast<Qt::ApplicationState>(3)); // unnamed but valid enum-range value
  fixture.host->pumpEvents();
  fixture.assertJS("p.current === 'unknown'");
  fixture.evaluate(R"JS(
    globalThis.module = globalThis.nativeModuleProxy.AppState;
    let snapshot;
    module.getCurrentAppState(value => { snapshot = value.app_state; }, () => {});
    if (snapshot !== 'unknown') throw new Error('Cached state mismatch');
    const invalid = [
      () => module.getConstants(1),
      () => module.getCurrentAppState(),
      () => module.getCurrentAppState(null, () => {}),
      () => module.getCurrentAppState(() => {}, null),
      () => module.addListener(),
      () => module.addListener('memoryWarning'),
      () => module.removeListeners(-1),
      () => module.removeListeners(0.5),
      () => module.removeListeners(Infinity),
      () => module.removeListeners('1'),
      () => p.invalidEvent(),
    ];
    for (const call of invalid) {
      let threw = false;
      try { call(); } catch (error) { threw = error instanceof Error; }
      if (!threw) throw new Error('Invalid native or public call did not reject');
    }
    module.addListener('appStateDidChange');
    module.removeListeners(1);
    module.removeListeners(100);
    let propagated = false;
    try { module.getCurrentAppState(() => { throw new Error('sentinel'); }, () => {}); }
    catch (error) { propagated = error.message.includes('sentinel'); }
    if (!propagated) throw new Error('Callback exception was swallowed');
  )JS");
  auto queue = fixture.host->taskQueue();
  notify(Qt::ApplicationHidden);
  fixture.host.reset();
  QCoreApplication::sendPostedEvents();
  bool rejected = false;
  try { queue->post([] {}); } catch (const std::runtime_error&) { rejected = true; }
  require(rejected, "Runtime task admission remained open after destruction");
#else
  fixture.assertJS("p.available === false && p.current === null");
  fixture.evaluate(R"JS(
    let rejected = false;
    try { p.subscribe(); } catch (error) { rejected = error.message.includes('isAvailable'); }
    if (!rejected) throw new Error('Disabled native capability was not explicit');
  )JS");
#endif
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try {
    require(argc == 3, "Usage: appstate-runtime <production-bundle> <qnx|marathon>");
    const std::string platform(argv[2]);
    require(platform == "qnx" || platform == "marathon", "Unexpected platform identity");
    QFile input(QString::fromLocal8Bit(argv[1]));
    require(input.open(QIODevice::ReadOnly), "Cannot open production Metro fixture");
    const auto bundle = input.readAll().toStdString();
    require(!bundle.empty(), "Empty production Metro fixture");
    for (int generation = 0; generation < 4; ++generation) publicContract(bundle, platform);
#if RNQNX_ENABLE_APP_STATE
    pendingDeliveriesCannotOutliveObservation();
    // aboutToQuit closes the producer without simulating OS authorization.
    Fixture quit;
    require(quit.host->evaluate(bundle, "quit.bundle.js"), quit.host->lastError().toStdString());
    require(QMetaObject::invokeMethod(qGuiApp, "aboutToQuit", Qt::DirectConnection), "Quit signal absent");
    quit.evaluate(R"JS(
      let rejected = false;
      try { nativeModuleProxy.AppState.getConstants(); } catch (error) { rejected = error.message.includes('closed'); }
      if (!rejected) throw new Error('Quit did not invalidate native observation');
    )JS");
#endif
    std::printf("RNQNX_APPSTATE_RUNTIME_OK platform=%s enabled=%d generations=4; Linux Qt/Hermes, synthetic OS states only\n",
        platform.c_str(), RNQNX_ENABLE_APP_STATE);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "RNQNX_APPSTATE_TEST_ERROR %s\n", error.what());
    return 1;
  }
}
