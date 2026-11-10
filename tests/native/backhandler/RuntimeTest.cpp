#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#if RNQNX_ENABLE_BACK_HANDLER
#include "react-native-qnx/runtime/QnxBackHandlerModule.h"
#include <ReactCommon/CallInvoker.h>
#endif
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
namespace jsi = facebook::jsi;
void require(bool result, const std::string& message) {
  if (!result) throw std::runtime_error(message);
}
class Window final : public QQuickWindow {
 public:
  int closeRequests{0};
  bool event(QEvent* event) override {
    if (event->type() == QEvent::Close) {
      ++closeRequests;
      event->ignore(); // Count real close requests without ending the test app.
      return true;
    }
    return QQuickWindow::event(event);
  }
};
struct Fixture {
  QQmlEngine engine;
  rnqnx::views::QtQuickItemFactory factory{engine};
  Window window;
  rnqnx::fabric::QtQuickMountingLayer layer{factory, *window.contentItem(), 1};
  rnqnx::fabric::ShadowViewMutationAdapter adapter{layer};
  std::unique_ptr<rnqnx::runtime::QtReactHost> host{
      std::make_unique<rnqnx::runtime::QtReactHost>(adapter)};
  Fixture() {
    window.show();
    host->installTurboModuleBinding();
    host->installComponentProvider();
    host->installPlatformConstants();
    host->installTimers();
#if RNQNX_ENABLE_BACK_HANDLER
    host->setBackHandlerWindow(&window);
#endif
  }
  void evaluate(const std::string& source) {
    require(host->evaluate(source, "backhandler-test.js"), host->lastError().toStdString());
  }
  void assertJS(const std::string& source) {
    evaluate("if (!(" + source + ")) throw new Error('BackHandler assertion failed');");
  }
};
void back(Window& window, bool repeat = false, int key = Qt::Key_Back) {
  QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(), repeat);
  QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(), repeat);
  QCoreApplication::sendEvent(&window, &press);
  QCoreApplication::sendEvent(&window, &release);
}
#if RNQNX_ENABLE_BACK_HANDLER
class DeferredInvoker final : public facebook::react::CallInvoker {
 public:
  void invokeAsync(facebook::react::CallFunc&& function) noexcept override {
    tasks.push_back(std::move(function));
  }
  void invokeSync(facebook::react::CallFunc&&) override { throw std::logic_error("No sync hop"); }
  void drain(jsi::Runtime& runtime) {
    auto pending = std::move(tasks);
    tasks.clear();
    for (auto& task : pending) task(runtime);
  }
  std::vector<facebook::react::CallFunc> tasks;
};
void lifetime() {
  Fixture fixture;
  fixture.host->setBackHandlerWindow(nullptr);
  fixture.evaluate("globalThis.events = 0; globalThis.__rctDeviceEventEmitter = {listenerCount() { return 1; }, emit() { events++; }};");
  auto invoker = std::make_shared<DeferredInvoker>();
  auto module = std::make_shared<rnqnx::runtime::QnxBackHandlerModule>(invoker);
  module->setWindow(&fixture.window);
  back(fixture.window);
  require(invoker->tasks.size() == 1, "Expected one deferred Back event");
  module->setWindow(nullptr);
  invoker->drain(fixture.host->runtime());
  fixture.assertJS("events === 0");
  require(fixture.window.closeRequests == 0, "Cancelled delivery performed fallback");
  module->setWindow(&fixture.window);
  back(fixture.window);
  module->invalidate();
  module->invalidate();
  invoker->drain(fixture.host->runtime());
  fixture.assertJS("events === 0");
  require(fixture.window.closeRequests == 0, "Cancelled delivery performed fallback");
  back(fixture.window);
  require(invoker->tasks.empty(), "Closed observation still queued work");
  // TurboModule registry may release its final reference off-owner after the
  // host has invalidated it. No Qt observer may survive into that destruction.
  std::thread release([module = std::move(module)]() mutable { module.reset(); });
  release.join();

  auto second = std::make_shared<rnqnx::runtime::QnxBackHandlerModule>(invoker);
  auto temporary = std::make_unique<Window>();
  second->setWindow(temporary.get());
  back(*temporary);
  temporary.reset();
  invoker->drain(fixture.host->runtime());
  fixture.assertJS("events === 0");
  require(fixture.window.closeRequests == 0, "Cancelled delivery performed fallback");
  second->setWindow(&fixture.window);
  back(fixture.window);
  Window replacement;
  second->setWindow(&replacement);
  invoker->drain(fixture.host->runtime());
  fixture.assertJS("events === 0");
  require(fixture.window.closeRequests == 0, "Cancelled delivery performed fallback");
  second->invalidate();
}
#endif
void contract(const std::string& bundle, const std::string& platform) {
  Fixture fixture;
  fixture.evaluate(bundle);
  fixture.evaluate("globalThis.p = globalThis.__rnqnxBackHandlerProbe;");
  fixture.assertJS("p.platform === '" + platform + "'");
#if RNQNX_ENABLE_BACK_HANDLER
  fixture.assertJS("p.available && !p.error");
  fixture.evaluate("p.configure(true);");
  back(fixture.window);
  fixture.host->pumpEvents();
  fixture.assertJS("JSON.stringify(p.calls) === '[\"last\"]'");
  require(fixture.window.closeRequests == 0, "Consumed Back closed the window");
  fixture.evaluate("p.configure(false);");
  back(fixture.window);
  fixture.host->pumpEvents();
  fixture.assertJS("JSON.stringify(p.calls) === '[\"last\",\"first\"]'");
  require(fixture.window.closeRequests == 1, "Unhandled Back must close exactly once");
  fixture.evaluate("p.configure(true); p.removeLast();");
  back(fixture.window);
  fixture.host->pumpEvents();
  fixture.assertJS("JSON.stringify(p.calls) === '[\"first\"]'");
  require(fixture.window.closeRequests == 2, "Removing handler did not restore fallback");
  fixture.evaluate("p.configure(true);");
  Window unrelated;
  unrelated.show();
  back(unrelated);
  back(fixture.window, true);
  back(fixture.window, false, Qt::Key_Escape);
  fixture.host->pumpEvents();
  fixture.assertJS("p.calls.length === 0");
  require(unrelated.closeRequests == 0, "Cross-window navigation");
  fixture.evaluate("p.removeAll(); p.exit();");
  require(fixture.window.closeRequests == 3, "Explicit exit did not close attached window");
  fixture.evaluate("p.configureDoubleExit();");
  back(fixture.window);
  fixture.host->pumpEvents();
  require(fixture.window.closeRequests == 4, "Duplicate JS/default fallback closed twice");
  fixture.host->setBackHandlerWindow(nullptr);
  fixture.evaluate("let rejected = false; try { p.exit(); } catch (e) { rejected = /attached live/.test(e.message); } if (!rejected) throw new Error('Detached exit must fail');");
  fixture.host->setBackHandlerWindow(&fixture.window);
  fixture.evaluate("globalThis.__rctDeviceEventEmitter.removeAllListeners('hardwareBackPress');");
  back(fixture.window);
  fixture.host->pumpEvents();
  require(fixture.window.closeRequests == 5, "Emitter without RN Back listener swallowed Back");
  fixture.evaluate("delete globalThis.__rctDeviceEventEmitter;");
  back(fixture.window);
  fixture.host->pumpEvents();
  require(fixture.window.closeRequests == 6, "Absent emitter swallowed Back");
#else
  fixture.assertJS("!p.available && /RNQNX_ENABLE_BACK_HANDLER/.test(p.error)");
  back(fixture.window);
  fixture.host->pumpEvents();
  require(fixture.window.closeRequests == 0, "OFF feature changed Back behavior");
#endif
}
}  // namespace
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  try {
    require(argc == 3, "Usage: runtime-test bundle platform");
    QFile file(QString::fromLocal8Bit(argv[1]));
    require(file.open(QIODevice::ReadOnly), "Cannot read bundle");
    contract(file.readAll().toStdString(), argv[2]);
#if RNQNX_ENABLE_BACK_HANDLER
    lifetime();
#endif
    std::puts("RNQNX_BACK_HANDLER_RUNTIME_OK (synthetic Qt input, not physical QNX proof)");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
