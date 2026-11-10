#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#if RNQNX_ENABLE_LINKING
#include "react-native-qnx/runtime/QnxLinkingModule.h"
#include <ReactCommon/CallInvoker.h>
#endif
#include <QFile>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>
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
struct Fixture {
  QQmlEngine engine;
  rnqnx::views::QtQuickItemFactory factory{engine};
  QQuickItem root;
  rnqnx::fabric::QtQuickMountingLayer layer{factory, root, 1};
  rnqnx::fabric::ShadowViewMutationAdapter adapter{layer};
  std::unique_ptr<rnqnx::runtime::QtReactHost> host{
      std::make_unique<rnqnx::runtime::QtReactHost>(adapter)};
  Fixture() {
    host->installTurboModuleBinding();
    host->installComponentProvider();
    host->installPlatformConstants();
    host->installTimers();
  }
  void evaluate(const std::string& source) {
    require(host->evaluate(source, "linking-test.js"), host->lastError().toStdString());
  }
  void assertJS(const std::string& source) {
    evaluate("if (!(" + source + ")) throw new Error('Linking assertion failed');");
  }
};
void activate(const QString& url) {
  QFileOpenEvent event(QUrl(url, QUrl::StrictMode));
  QCoreApplication::sendEvent(qGuiApp, &event);
}
#if RNQNX_ENABLE_LINKING
class DeferredInvoker final : public facebook::react::CallInvoker {
 public:
  void invokeAsync(facebook::react::CallFunc&& task) noexcept override { tasks.push_back(std::move(task)); }
  void invokeSync(facebook::react::CallFunc&&) override { throw std::logic_error("No sync hop"); }
  void drain(jsi::Runtime& runtime) {
    auto pending = std::move(tasks);
    tasks.clear();
    for (auto& task : pending) task(runtime);
  }
  std::vector<facebook::react::CallFunc> tasks;
};
void lifetimeAndBounds() {
  Fixture fixture;
  fixture.evaluate("globalThis.events = 0; globalThis.__rctDeviceEventEmitter = {emit() { events++; }};");
  auto invoker = std::make_shared<DeferredInvoker>();
  auto module = std::make_shared<rnqnx::runtime::QnxLinkingModule>(invoker);
  module->configure({"rnqnx"}, std::nullopt);
  activate(QStringLiteral("rnqnx://room/42"));
  require(invoker->tasks.size() == 1, "Negative lifetime control needs accepted work");
  module->invalidate();
  module->invalidate();
  invoker->drain(fixture.host->runtime());
  fixture.assertJS("events === 0");
  std::thread release([module = std::move(module)]() mutable { module.reset(); });
  release.join();
  activate(QStringLiteral("rnqnx://room/42"));
  require(invoker->tasks.empty(), "Destroyed observer still queued work");
  auto next = std::make_shared<rnqnx::runtime::QnxLinkingModule>(invoker);
  next->configure({"rnqnx"}, std::nullopt);
  for (int i = 0; i != 40; ++i) activate(QStringLiteral("rnqnx://room/42"));
  require(invoker->tasks.size() == 32, "Inbound event queue must be bounded");
  invoker->drain(fixture.host->runtime());
  fixture.assertJS("events === 32");
  activate(QStringLiteral("rnqnx://room/43"));
  require(invoker->tasks.size() == 1, "Draining must restore capacity");
  next->invalidate();
  invoker->drain(fixture.host->runtime());
  fixture.assertJS("events === 32");
}
void configuration() {
  Fixture fixture;
  for (const auto& schemes : std::vector<std::vector<std::string>>{{}, {"https"}, {"file"}, {"bad scheme"}}) {
    bool refused = false;
    try { fixture.host->configureLinking(schemes); } catch (const std::invalid_argument&) { refused = true; }
    require(refused, "Unsafe scheme configuration accepted");
  }
  for (const auto& url : {"other://room/42", "rnqnx://user:secret@room/42", "relative/path", "rnqnx://room/%"}) {
    bool refused = false;
    try { fixture.host->configureLinking({"rnqnx"}, std::string(url)); }
    catch (const std::invalid_argument&) { refused = true; }
    require(refused, "Unsafe initial URL accepted");
  }
  fixture.host->configureLinking({"rnqnx"});
  bool refused = false;
  try { fixture.host->configureLinking({"rnqnx"}); } catch (const std::logic_error&) { refused = true; }
  require(refused, "Launch snapshot must be immutable");
  Fixture other;
  refused = false;
  try { other.host->configureLinking({"other"}); } catch (const std::logic_error&) { refused = true; }
  require(refused, "Second runtime took over application activation");
  fixture.host.reset();
  other.host->configureLinking({"other"}); // Old owner must release registration.
}
#endif
void contract(const std::string& bundle, const std::string& platform, bool launch) {
  Fixture fixture;
#if RNQNX_ENABLE_LINKING
  if (launch) fixture.host->configureLinking({"rnqnx"}, std::string("rnqnx://room/42?mode=guest"));
#else
  (void)launch;
#endif
  fixture.evaluate(bundle);
  fixture.evaluate("globalThis.p = globalThis.__rnqnxLinkingProbe;");
  fixture.host->pumpEvents();
  fixture.assertJS("p.platform === '" + platform + "'");
#if RNQNX_ENABLE_LINKING
  fixture.assertJS("p.available && !p.error && p.failuresSettled && p.failures.length === 3 && p.failures.every(Boolean)");
  fixture.assertJS(launch ? "p.initial === 'rnqnx://room/42?mode=guest' && p.repeated === p.initial" : "p.initial === null && p.repeated === null");
  if (!launch) return;
  activate(QStringLiteral("other://room/1"));
  activate(QStringLiteral("https://example.com/room/1"));
  activate(QStringLiteral("rnqnx://user:secret@room/1"));
  activate(QStringLiteral("rnqnx://room/") + QString(9000, QChar('a')));
  QFileOpenEvent file(QStringLiteral("/tmp/room"));
  QCoreApplication::sendEvent(qGuiApp, &file);
  fixture.host->pumpEvents();
  fixture.assertJS("p.first.length === 0 && p.second.length === 0");
  activate(QStringLiteral("rnqnx://room/43?mode=guest"));
  fixture.assertJS("p.first.length === 0 && p.second.length === 0"); // No inline JS in Qt filter.
  fixture.host->pumpEvents();
  fixture.assertJS("p.first[0] === 'rnqnx://room/43?mode=guest' && JSON.stringify(p.first) === JSON.stringify(p.second)");
  fixture.evaluate("p.removeFirst(); p.clear();");
  activate(QStringLiteral("rnqnx://room/44"));
  fixture.host->pumpEvents();
  fixture.assertJS("p.first.length === 0 && p.second[0] === 'rnqnx://room/44'");
  fixture.evaluate("p.removeAll(); p.clear();");
  activate(QStringLiteral("rnqnx://room/45"));
  fixture.host->pumpEvents();
  fixture.assertJS("p.first.length === 0 && p.second.length === 0");
  fixture.evaluate("globalThis.badURL = p.addThrowingListener();");
  const auto errorsBefore = fixture.host->javaScriptErrors().size();
  activate(QStringLiteral("rnqnx://room/46"));
  require(fixture.host->javaScriptErrors().size() == errorsBefore, "URL handler executed inside Qt filter");
  fixture.host->pumpEvents();
  require(fixture.host->javaScriptErrors().size() > errorsBefore, "Throwing URL handler was not reported");
  fixture.evaluate("badURL.remove();");
#else
  fixture.assertJS("!p.available && /RNQNX_ENABLE_LINKING/.test(p.error)");
  activate(QStringLiteral("rnqnx://room/42"));
  fixture.host->pumpEvents();
  fixture.assertJS("p.first.length === 0 && p.second.length === 0");
#endif
}
}  // namespace
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    require(argc == 3, "Usage: runtime-test bundle platform");
    QFile file(QString::fromLocal8Bit(argv[1]));
    require(file.open(QIODevice::ReadOnly), "Cannot read bundle");
    const auto bundle = file.readAll().toStdString();
    contract(bundle, argv[2], false);
    contract(bundle, argv[2], true);
#if RNQNX_ENABLE_LINKING
    lifetimeAndBounds();
    configuration();
#endif
    std::puts("RNQNX_LINKING_RUNTIME_OK (synthetic Qt activation, not OS/device proof)");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
