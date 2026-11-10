#include "react-native-qnx/runtime/DevSession.h"
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using rnqnx::runtime::DevSession;
using rnqnx::runtime::QtReactHost;

namespace {
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void until(const std::function<bool()>& predicate, const char* message) {
  QElapsedTimer elapsed;
  elapsed.start();
  while (!predicate() && elapsed.elapsed() < 4000) {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  check(predicate(), message);
}

struct HttpFixture {
  QTcpServer server;
  int requests{0};
  std::function<void(QTcpSocket*, int)> respond;
  HttpFixture() {
    check(server.listen(QHostAddress::LocalHost, 0), "HTTP fixture listens");
    QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
      while (auto* socket = server.nextPendingConnection()) {
        socket->setParent(&server);
        auto request = std::make_shared<QByteArray>();
        QObject::connect(socket, &QIODevice::readyRead, &server, [this, socket, request] {
          request->append(socket->readAll());
          if (!request->contains("\r\n\r\n") || socket->property("handled").toBool()) return;
          socket->setProperty("handled", true);
          respond(socket, ++requests);
        });
      }
    });
  }
  static void send(QTcpSocket* socket, const QByteArray& body, int code = 200) {
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
    socket->write("HTTP/1.1 " + QByteArray::number(code) + " Result\r\nContent-Type: application/javascript\r\nContent-Length: " +
                  QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
    socket->disconnectFromHost();
  }
  DevSession::Configuration configuration() const {
    DevSession::Configuration config;
    config.bundleUrl = QUrl(QStringLiteral("http://127.0.0.1:%1/index.bundle?dev=true&hot=true&platform=qnx").arg(server.serverPort()));
    config.componentName = "SyntheticLifecycleOnly";
    return config;
  }
};
// Synthetic bootstrap tests native lifecycle only, not RN Refresh or rendering.
const QByteArray bundle = "globalThis.__RNQNX_DEV_BOOTSTRAPPED__=true;globalThis.counter=0;"
                         "globalThis.__RNQNX_DEV_DISPOSE__=()=>recordDisposal();";

double metric(QtReactHost& host, const char* snapshot, const char* group, const char* field) {
  auto& runtime = host.runtime();
  return runtime.global().getPropertyAsObject(runtime, "__nativeMetrics")
      .getPropertyAsObject(runtime, snapshot).getPropertyAsObject(runtime, group)
      .getProperty(runtime, field).getNumber();
}

void nativeWindowMetrics() {
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(800, 600);
  QQuickItem client(window.contentItem());
  client.setSize(QSizeF(0, 0));
  rnqnx::views::QtQuickItemFactory factory(engine);
  rnqnx::fabric::QtQuickMountingLayer layer(factory, client, 31);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  HttpFixture http;
  auto config = http.configuration();
  config.surfaceId = 31;
  auto incomplete = config;
  incomplete.window = &window;
  check(!DevSession::validate(incomplete).isEmpty(), "native metrics reject an incomplete window/client pair");
  // Negative control reproduces the original manual-size path: its bundle sees
  // zero initial DeviceInfo geometry, so the initial metrics assertion must fail.
  const bool unobservedControl = qEnvironmentVariableIsSet("RNQNX_DEV_METRICS_UNOBSERVED_CONTROL");
  if (!unobservedControl) {
    config.window = &window;
    config.client = &client;
  }
  http.respond = [](QTcpSocket* socket, int request) {
    // The bootstrap/AppRegistry stand-ins isolate native lifecycle. DeviceInfo,
    // Hermes, Fabric surface constraints and Qt observations are production code.
    const QByteArray source = R"JS(
globalThis.__RNQNX_DEV_BOOTSTRAPPED__ = true;
globalThis.__nativeMetrics = {
  initial: nativeModuleProxy.DeviceInfo.getConstants().Dimensions,
  latest: null, changes: 0, starts: 0
};
globalThis.__rctDeviceEventEmitter = {emit(name, dimensions) {
  if (name === 'didUpdateDimensions') {
    __nativeMetrics.latest = dimensions;
    __nativeMetrics.changes++;
  }
}};
globalThis.RN$AppRegistry = {runApplication() { __nativeMetrics.starts++; }};
globalThis.RN$stopSurface = () => {};
)JS";
    HttpFixture::send(socket, source + "__nativeMetrics.sourceRequest=" + QByteArray::number(request) + ";");
  };
  QtReactHost* host = nullptr;
  int creations = 0;
  DevSession session(adapter, config, [&](QtReactHost& current) {
    host = &current;
    ++creations;
  });
  if (unobservedControl) session.resize(713.25F, 431.5F);
  session.start();
  if (!unobservedControl) {
    until([&] { return http.requests == 1 && session.status().phase == DevSession::Phase::Loading; },
          "fetched bundle waits for native bounds");
    check(creations == 0, "zero client bounds defer runtime creation and bundle evaluation");
    session.stop();
    client.setSize(QSizeF(100, 100));
    QCoreApplication::processEvents();
    check(creations == 0 && session.status().phase == DevSession::Phase::Stopped,
          "positive geometry cannot evaluate a stopped generation");

    client.setSize(QSizeF(0, 0));
    session.start();
    until([&] { return http.requests == 2 && session.status().phase == DevSession::Phase::Loading; },
          "restarted bundle waits for native bounds");
    session.requestReload(QStringLiteral("replace geometry-blocked source"));
    until([&] { return http.requests == 3 && session.status().phase == DevSession::Phase::Loading; },
          "replacement fetch waits for native bounds");
    check(creations == 0, "blocked reload has not evaluated either source generation");
    client.setSize(QSizeF(713.25, 431.5));
  }
  until([&] { return creations == 1 && host->runtime().global().hasProperty(host->runtime(), "__nativeMetrics"); },
        "native metrics bundle evaluated");
  check(metric(*host, "initial", "window", "width") == 713.25 &&
        metric(*host, "initial", "window", "height") == 431.5,
        "dev bundle sees native client dimensions before surface startup");
  check(host->runtime().global().getPropertyAsObject(host->runtime(), "__nativeMetrics")
        .getProperty(host->runtime(), "sourceRequest").getNumber() == 3,
        "positive geometry evaluates only the latest fetched generation");
  check(std::abs(metric(*host, "initial", "window", "scale") - window.devicePixelRatio()) < 0.001,
        "dev bundle sees actual window pixel ratio");
  check(window.screen() &&
        metric(*host, "initial", "screen", "width") == window.screen()->geometry().width(),
        "dev bundle sees actual window screen metrics");
  until([&] { return session.status().phase == DevSession::Phase::Ready; }, "native geometry surface starts");
  bool manualResizeRejected = false;
  try { session.resize(640, 480); } catch (const std::logic_error&) { manualResizeRejected = true; }
  check(manualResizeRejected, "native geometry session rejects a second resize owner");

  client.setSize(QSizeF(638.5, 377.25));
  until([&] {
    auto& runtime = host->runtime();
    return runtime.global().getPropertyAsObject(runtime, "__nativeMetrics")
        .getProperty(runtime, "changes").getNumber() > 0;
  }, "native geometry updates DeviceInfo");
  check(metric(*host, "latest", "window", "width") == 638.5 &&
        metric(*host, "latest", "window", "height") == 377.25,
        "dev window resize publishes current client dimensions");
  check(session.status().phase == DevSession::Phase::Ready,
        "native resize does not invoke the rejected manual surface resize");

  // Reload while an observation is queued. The new runtime must bind current
  // metrics before evaluating; the old observer must not deliver into it.
  client.setSize(QSizeF(604.25, 399.5));
  session.requestReload(QStringLiteral("native metrics generation"));
  until([&] { return creations == 2 && host->runtime().global().hasProperty(host->runtime(), "__nativeMetrics"); },
        "replacement metrics bundle evaluated");
  check(metric(*host, "initial", "window", "width") == 604.25 &&
        metric(*host, "initial", "window", "height") == 399.5,
        "replacement bundle receives current native dimensions");
  client.setSize(QSizeF(690.5, 412.25));
  until([&] {
    auto& runtime = host->runtime();
    return runtime.global().getPropertyAsObject(runtime, "__nativeMetrics")
        .getProperty(runtime, "changes").getNumber() > 0;
  }, "replacement observer updates DeviceInfo");
  check(metric(*host, "latest", "window", "width") == 690.5 &&
        metric(*host, "latest", "window", "height") == 412.25,
        "replacement observer publishes the latest native geometry");
  auto& runtime = host->runtime();
  check(runtime.global().getPropertyAsObject(runtime, "__nativeMetrics")
        .getProperty(runtime, "changes").getNumber() == 1,
        "replacement runtime has one coalesced geometry producer");
  session.stop();
  client.setWidth(700);
  QCoreApplication::processEvents();
  std::cout << "RNQNX_DEV_WINDOW_METRICS_OK deferred-start cancelled-source initial resize reload single-owner\n";
}
}

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  nativeWindowMetrics();
  QQmlEngine engine;
  QQuickItem root;
  rnqnx::views::QtQuickItemFactory factory(engine);
  rnqnx::fabric::QtQuickMountingLayer layer(factory, root, 1);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  HttpFixture http;
  auto config = http.configuration();
  check(DevSession::validate(config).isEmpty(), "explicit development URL accepted");
  for (const char* query : {"dev=false&hot=true&platform=qnx", "dev=true&hot=false&platform=qnx",
                           "dev=true&hot=true&platform=ios", "dev=true&dev=true&hot=true&platform=qnx"}) {
    auto invalid = config;
    invalid.bundleUrl.setQuery(QString::fromLatin1(query));
    check(!DevSession::validate(invalid).isEmpty(), "wrong/duplicate development query rejected");
  }
  auto invalid = config;
  invalid.bundleUrl.setUserName(QStringLiteral("secret"));
  check(!DevSession::validate(invalid).isEmpty(), "URL credentials rejected");
  invalid = config;
  invalid.maxFetchAttempts = 6;
  check(!DevSession::validate(invalid).isEmpty(), "unbounded retries rejected");

  int disposals = 0;
  int creations = 0;
  QtReactHost* host = nullptr;
  auto prepare = [&](QtReactHost& current) {
    host = &current;
    ++creations;
    auto& runtime = current.runtime();
    runtime.global().setProperty(runtime, "recordDisposal",
        facebook::jsi::Function::createFromHostFunction(runtime,
            facebook::jsi::PropNameID::forAscii(runtime, "recordDisposal"), 0,
            [&](facebook::jsi::Runtime&, const facebook::jsi::Value&, const facebook::jsi::Value*, size_t) {
              ++disposals;
              return facebook::jsi::Value::undefined();
            }));
  };
  http.respond = [](QTcpSocket* socket, int) { HttpFixture::send(socket, bundle); };
  {
    DevSession session(adapter, config, prepare);
    int statusDeliveries = 0;
    const auto subscription = session.addStatusListener([&](const DevSession::Status&) { ++statusDeliveries; });
    session.start();
    until([&] { return creations == 1 && host->runtime().global().hasProperty(host->runtime(), "counter"); }, "initial asynchronous evaluation");
    const auto generation = session.status().generation;
    check(host->runtime().global().getPropertyAsObject(host->runtime(), "__RNQNX_DEV_SESSION__")
        .getProperty(host->runtime(), "bundleUrl").isString(), "immutable bundle identity installed");
    check(host->evaluate("globalEvalWithSourceUrl('globalThis.counter += 1', 'http://localhost/module.js');", "probe"), "source-enabled module injection");
    check(host->runtime().global().getProperty(host->runtime(), "counter").getNumber() == 1, "same runtime state updated");
    check(!host->evaluate("throw new Error('recoverable')", "error.js"), "evaluation error returned");
    check(host->evaluate("globalThis.counter += 1", "correction.js"), "runtime accepts correction");
    auto oldQueue = host->taskQueue();
    int accepted = 0;
    oldQueue->post([&] { ++accepted; });
    // Request reload from inside a real native method entered by Hermes.
    auto& rt = host->runtime();
    rt.global().setProperty(rt, "requestReload",
        facebook::jsi::Function::createFromHostFunction(rt,
            facebook::jsi::PropNameID::forAscii(rt, "requestReload"), 0,
            [&](facebook::jsi::Runtime&, const facebook::jsi::Value&, const facebook::jsi::Value*, size_t) {
              session.requestReload(QStringLiteral("test native callback"));
              check(session.status().generation == generation, "reload waits for JS idle boundary");
              return facebook::jsi::Value::undefined();
            }));
    check(host->evaluate("requestReload()", "reload.js"), "reload request returns from JS");
    until([&] { return creations == 2 && host->runtime().global().hasProperty(host->runtime(), "counter"); }, "replacement runtime evaluated");
    check(session.status().generation > generation, "explicit reload changes runtime generation");
    check(host->runtime().global().getProperty(host->runtime(), "counter").getNumber() == 0, "full reload resets state");
    check(disposals == 1 && accepted == 1, "producer disposed and accepted queue work drained once");
    bool rejected = false;
    try { oldQueue->post([] {}); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "late completion rejected by old queue");
    subscription.remove();
    const auto deliveries = statusDeliveries;
    session.stop();
    const auto stopped = session.status().generation;
    session.stop();
    QCoreApplication::processEvents();
    check(session.status().generation == stopped && disposals == 2, "repeated stop is idempotent");
    check(statusDeliveries == deliveries, "removed listener receives no later status");
  }

  for (const bool failAfterStop : {false, true}) {
    int preparations = 0;
    DevSession* preparingSession = nullptr;
    DevSession cancelled(adapter, config, [&](QtReactHost&) {
      ++preparations;
      preparingSession->stop();
      if (failAfterStop) throw std::runtime_error("stopped preparation");
    });
    preparingSession = &cancelled;
    cancelled.start();
    until([&] { return preparations == 1; }, "preparation cancels its generation");
    QCoreApplication::processEvents();
    check(cancelled.status().phase == DevSession::Phase::Stopped,
          "stopped preparation cannot evaluate a bundle or revive status");
  }

  {
    const int before = creations;
    QPointer<QTcpSocket> stale;
    int attempts = 0;
    http.respond = [&](QTcpSocket* socket, int) {
      if (++attempts == 1) stale = socket;
      else HttpFixture::send(socket, bundle);
    };
    DevSession session(adapter, config, prepare);
    session.start();
    until([&] { return attempts == 1; }, "first request held in flight");
    session.requestReload(QStringLiteral("cancel pending fetch"));
    until([&] { return attempts == 2 && creations == before + 1; }, "new generation fetch completes");
    const auto generation = session.status().generation;
    HttpFixture::send(stale, "throw new Error('stale request must not evaluate')");
    QCoreApplication::processEvents();
    check(session.status().generation == generation && creations == before + 1, "stale reply ignored");
    session.stop();
  }
  {
    auto limited = config;
    limited.maxFetchAttempts = 2;
    int attempts = 0;
    http.respond = [&](QTcpSocket* socket, int) {
      const int attempt = ++attempts;
      HttpFixture::send(socket, attempt == 1 ? QByteArray("compile error") : bundle, attempt == 1 ? 500 : 200);
    };
    const int before = creations;
    DevSession retry(adapter, limited, prepare);
    retry.start();
    until([&] { return attempts == 2 && creations == before + 1; }, "HTTP error retries then succeeds");
    retry.stop();
    attempts = 0;
    http.respond = [&](QTcpSocket*, int) { ++attempts; };
    limited.fetchTimeoutMs = 50;
    DevSession timeout(adapter, limited, prepare);
    timeout.start();
    until([&] { return attempts == 2 && timeout.status().phase == DevSession::Phase::Error; }, "timeout retries reach bounded error");
    timeout.stop();
    attempts = 0;
    http.respond = [&](QTcpSocket* socket, int) { ++attempts; HttpFixture::send(socket, QByteArray(128, 'x')); };
    limited.maxFetchAttempts = 1;
    limited.maxBundleBytes = 32;
    DevSession oversized(adapter, limited, prepare);
    oversized.start();
    until([&] { return oversized.status().phase == DevSession::Phase::Error; }, "oversized bundle rejected");
    oversized.stop();
  }
  std::cout << "RNQNX_DEV_SESSION_HOST_OK (synthetic lifecycle and native DeviceInfo metrics)\n";
}
