#include "QnxWindow.hpp"
#include "QnxNitroRuntime.hpp"
#include "QtNitroDispatcher.hpp"
#include "WindowSession.h"
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickWindow>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using rnqnx::runtime::QtReactHost;
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void evaluate(QtReactHost& host, const std::string& source) {
  if (!host.evaluate(source, "window-api-host-test.js"))
    throw std::runtime_error(host.lastError().toStdString());
}
void pumpUntil(QtReactHost& host, const std::function<bool()>& condition) {
  QElapsedTimer timer; timer.start();
  while (!condition() && timer.elapsed() < 5000) { host.pumpEvents(); QCoreApplication::processEvents(); }
  require(condition(), "Window API async boundary did not complete");
}
void exercise(bool jsThread, bool hidden) {
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(800, 600);
  window.show();
  rnqnx::window::WindowChromeConfig config;
  config.titleBarStyle = hidden ? rnqnx::window::TitleBarStyle::Hidden : rnqnx::window::TitleBarStyle::Default;
  auto caps = rnqnx::window::capabilitiesForPlatform("cocoa", {}, rnqnx::window::SurfaceResize::FollowsWindow);
  rnqnx::window::WindowChrome chrome(window, engine, config, caps);
  rnqnx::views::QtQuickItemFactory factory(engine);
  rnqnx::fabric::QtQuickMountingLayer layer(factory, chrome.contentRoot(), 1);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  QtReactHost host(adapter, QtReactHost::Options{.jsThread = jsThread});
  rnqnx::window::WindowSession session(window, &chrome, caps);
  host.runOnJsThread([&] {
    auto dispatcher = std::make_shared<rnqnx::nitro::QtNitroDispatcher>(host.taskQueue());
    rnqnx::nitro::installQnxNitroRuntime(host.runtime(), dispatcher);
    rnqnx::nitro::windowapi::installWindowHost(host, session.state());
  });
  evaluate(host, std::string("globalThis.expectedInset = ") + (hidden ? "44;" : "0;") +
      "globalThis.expectedContentHeight = " + (hidden ? "600;" : "556;"));
  evaluate(host, R"JS(
    // Each evaluate() is a separate Hermes program: reusable helpers must be
    // global object properties, rather than program-scoped lexical bindings.
    globalThis.check = (ok, message) => { if (!ok) throw new Error(message); };
    globalThis.first = __rnqnxWindowHost.getCurrentWindow();
    globalThis.second = __rnqnxWindowHost.getCurrentWindow();
    check(!first.equals(second), 'handles must be independent');
    check(first.snapshot.insets.top === expectedInset, 'inset missing from actual Nitro getter');
    check(first.snapshot.contentHeight === expectedContentHeight, 'surface size double-counted the title bar');
    globalThis.events = [];
    globalThis.observer = first.addSnapshotListener(value => events.push(value));
    globalThis.guard = first.addCloseGuard();
    globalThis.otherGuard = second.addCloseGuard();
    globalThis.result = 'pending';
    first.close().then(closed => { check(!closed, 'close guard failed'); result = 'vetoed'; });
  )JS");
  const auto resultIs = [&](const char* expected) {
    bool equal = false;
    host.runOnJsThread([&] {
      auto& rt = host.runtime();
      equal = rt.global().getProperty(rt, "result").asString(rt).utf8(rt) == expected;
    });
    return equal;
  };
  pumpUntil(host, [&] { return resultIs("vetoed"); });
  evaluate(host, R"JS(
    result = 'pending';
    const oldRequest = first.snapshot.closeRequestId;
    check(oldRequest.length > 0, 'native request identity missing');
    first.cancelCloseRequest(oldRequest).then(cancelled => {
      check(cancelled && !first.snapshot.closeRequested, 'cancel did not clear pending prompt');
      return first.close();
    }).then(closed => {
      check(!closed && first.snapshot.closeRequestId !== oldRequest, 'new native close reused a request');
      return first.cancelCloseRequest(oldRequest);
    }).then(stale => {
      check(!stale && first.snapshot.closeRequested, 'stale dialog cancellation affected new request');
      result = 'cancelled';
    });
  )JS");
  pumpUntil(host, [&] { return resultIs("cancelled"); });
  evaluate(host, R"JS(
    check(events.length >= 1 && events[0].insets.top === expectedInset, 'ordered initial native snapshot');
    check(events[events.length - 1].closeRequested, 'close request event missing');
    result = 'pending';
    first.setTitle('Saved document').then(() => first.setControlsLayout('windows')).then(() => {
      check(first.snapshot.title === 'Saved document', 'title round trip');
      check(first.snapshot.controlsLayout === 'windows', 'layout round trip');
      result = 'updated';
    });
  )JS");
  pumpUntil(host, [&] { return resultIs("updated"); });
  evaluate(host, R"JS(
    result = 'pending';
    first.setSize(NaN, 500).then(() => { throw new Error('NaN accepted'); }, error => {
      check(error instanceof Error, 'native rejection was not an Error'); result = 'invalid';
    });
  )JS");
  pumpUntil(host, [&] { return resultIs("invalid"); });
  evaluate(host, R"JS(
    observer.remove(); observer.remove();
    guard.remove(); first.dispose();
    result = 'pending';
    second.close().then(closed => { check(!closed, 'first disposal removed another handle guard'); result = 'independent'; });
  )JS");
  pumpUntil(host, [&] { return resultIs("independent"); });
  evaluate(host, R"JS(
    otherGuard.remove();
    result = 'pending';
    second.setSize(900, 650).then(() => second.close()).then(closed => {
      check(closed, 'close should proceed after all guards removed'); result = 'closed';
    });
  )JS");
  pumpUntil(host, [&] { return resultIs("closed"); });
  // Window callbacks never call JS synchronously. Block the JS owner while
  // native chrome changes layouts and a native close guard vetoes a close.
  if (jsThread) {
    window.show();
    auto guard = session.state()->guard();
    std::atomic<bool> started{false}, release{false};
    host.postToJsThread([&] { started = true; while (!release.load()) std::this_thread::yield(); });
    QElapsedTimer timer; timer.start();
    while (!started && timer.elapsed() < 5000) QCoreApplication::processEvents();
    require(started, "JS block did not start");
    require(chrome.setControlsLayout("macos"), "native chrome blocked with JS");
    require(!session.close(), "native guard entered busy JS or allowed close");
    release = true;
    host.runOnJsThread([] {});
    guard->store(false);
  }
  evaluate(host, R"JS(
    globalThis.queuedCallbacks = 0;
    globalThis.muted = second.addSnapshotListener(() => { ++queuedCallbacks; });
    muted.remove();
    for (let i = 0; i < 1000; ++i) { const removable = second.addCloseGuard(); removable.remove(); }
    result = 'pending';
    second.setTitle('must not run after shutdown').then(
      () => { throw new Error('shutdown accepted pending command'); },
      error => { check(error instanceof Error, 'shutdown rejection type'); result = 'shutdown'; }
    );
  )JS");
  session.shutdown();
  pumpUntil(host, [&] { return resultIs("shutdown"); });
  require(window.title() != QStringLiteral("must not run after shutdown"), "pending command changed window after shutdown");
  evaluate(host, R"JS(
    check(queuedCallbacks === 0, 'queued callback entered JS after subscription removal');
    let failed = false;
    try { void second.snapshot; } catch (error) { failed = error instanceof Error; }
    check(failed, 'closed owner allowed a snapshot read');
    second.dispose();
  )JS");
}
} // namespace
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    exercise(false, false);
    exercise(false, true);
    exercise(true, false);
    exercise(true, true);
    std::cout << "WINDOW_API_HERMES_OK handles insets layout guards errors disposal js-owner\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "WINDOW_API_HERMES_FAIL " << error.what() << '\n';
    return 1;
  }
}
