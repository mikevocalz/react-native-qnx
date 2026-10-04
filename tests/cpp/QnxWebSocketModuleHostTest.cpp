#include "react-native-qnx/runtime/QnxWebSocketModule.h"

#include <hermes/hermes.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QThread>
#ifdef RNQNX_ENABLE_WEBSOCKETS
#include <QWebSocket>
#include <QWebSocketServer>
#endif

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <iostream>
#include <vector>

namespace jsi = facebook::jsi;
namespace react = facebook::react;
using Module = rnqnx::runtime::QnxWebSocketModule;

class Invoker final : public react::CallInvoker {
 public:
  void invokeAsync(react::CallFunc&& call) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(std::move(call));
  }
  void invokeSync(react::CallFunc&&) override { std::abort(); }
  bool hasPending() {
    std::lock_guard<std::mutex> lock(mutex_);
    return !pending_.empty();
  }
  void drain(jsi::Runtime& rt) {
    if (std::this_thread::get_id() != owner_) std::abort();
    std::vector<react::CallFunc> calls;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      calls.swap(pending_);
    }
    for (auto& call : calls) call(rt);
  }
 private:
  const std::thread::id owner_{std::this_thread::get_id()};
  std::mutex mutex_;
  std::vector<react::CallFunc> pending_;
};

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
jsi::Value eval(jsi::Runtime& rt, const std::string& source) {
  return rt.evaluateJavaScript(std::make_unique<jsi::StringBuffer>(source), "websocket-test");
}
template <typename... Args>
void call(jsi::Runtime& rt, Module& module, const char* name, Args&&... args) {
  module.get(rt, jsi::PropNameID::forAscii(rt, name))
      .asObject(rt).asFunction(rt).call(rt, std::forward<Args>(args)...);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  auto runtime = facebook::hermes::makeHermesRuntime();
  auto& rt = *runtime;
  auto invoker = std::make_shared<Invoker>();
  auto module = std::make_unique<Module>(invoker);
  eval(rt, "globalThis.events = []; globalThis.__rctDeviceEventEmitter = {"
           "emit(name, event) { events.push({name, ...event}); }};");
  call(rt, *module, "addListener", jsi::String::createFromAscii(rt, "websocketOpen"));
  call(rt, *module, "removeListeners", 1);
  bool threw = false;
  try {
    call(rt, *module, "connect");
  } catch (const jsi::JSError&) {
    threw = true;
  }
  check(threw, "bad arity throws instead of indexing absent arguments");
#ifdef RNQNX_ENABLE_WEBSOCKETS
  QWebSocketServer server(QStringLiteral("jsi-loopback"), QWebSocketServer::NonSecureMode);
  check(server.listen(QHostAddress::LocalHost, 0), "listen");
  QObject::connect(&server, &QWebSocketServer::newConnection, &app, [&] {
    auto* peer = server.nextPendingConnection();
    peer->setParent(&server);
    QObject::connect(peer, &QWebSocket::textMessageReceived, peer,
        [peer](const QString& text) { peer->sendTextMessage(text); });
    QObject::connect(peer, &QWebSocket::binaryMessageReceived, peer,
        [peer](const QByteArray& data) { peer->sendBinaryMessage(data); });
  });
  const auto url = QStringLiteral("ws://127.0.0.1:%1/").arg(server.serverPort()).toStdString();
  const auto connect = [&](int id) {
    call(rt, *module, "connect", jsi::String::createFromUtf8(rt, url),
         jsi::Value::null(), jsi::Object(rt), id);
  };
  const auto until = [&](const std::string& predicate) {
    QElapsedTimer timer;
    timer.start();
    while (!eval(rt, predicate).getBool() && timer.elapsed() < 3000) {
      QCoreApplication::processEvents();
      invoker->drain(rt);
      QThread::msleep(1);
    }
    check(eval(rt, predicate).getBool(), predicate.c_str());
  };
  connect(1);
  check(eval(rt, "events.length === 0").getBool(), "events are asynchronous");
  until("events.length === 1");
  check(eval(rt, "events[0].name === 'websocketOpen' && events[0].id === 1 && "
                 "events[0].protocol === ''").getBool(), "open event payload");
  call(rt, *module, "send", jsi::String::createFromAscii(rt, "hello"), 1);
  call(rt, *module, "sendBinary", jsi::String::createFromAscii(rt, "AP9/"), 1);
  call(rt, *module, "ping", 1);
  until("events.length === 3");
  check(eval(rt, "events[1].name === 'websocketMessage' && events[1].type === 'text' && "
                 "events[1].data === 'hello' && events[2].id === 1 && "
                 "events[2].type === 'binary' && events[2].data === 'AP9/'").getBool(),
        "RN text and binary event shape");
  call(rt, *module, "close", 3001, jsi::String::createFromAscii(rt, "done"), 1);
  until("events.length === 4");
  check(eval(rt, "events[3].name === 'websocketClosed' && events[3].code === 3001 && "
                 "events[3].reason === 'done'").getBool(), "RN close event shape");
  call(rt, *module, "connect", jsi::String::createFromAscii(rt, "http://invalid.test"),
       jsi::Value::null(), jsi::Object(rt), 2);
  until("events.length === 5");
  check(eval(rt, "events[4].name === 'websocketFailed' && events[4].id === 2 && "
                 "typeof events[4].message === 'string'").getBool(), "RN failure event shape");
  threw = false;
  try {
    call(rt, *module, "ping", -1);
  } catch (const jsi::JSError&) {
    threw = true;
  }
  check(threw, "invalid socket ID rejected");
  // Queue a native failure, leave its JS callback pending, then invalidate.
  call(rt, *module, "ping", 999);
  QElapsedTimer timer;
  timer.start();
  while (!invoker->hasPending() && timer.elapsed() < 3000) {
    QCoreApplication::processEvents();
  }
  check(invoker->hasPending(), "callback queued before destruction");
  connect(3); // Also leave a native command pending at destruction.
  module.reset();
  invoker->drain(rt);
  QCoreApplication::processEvents();
  check(eval(rt, "events.length === 5").getBool(), "queued callbacks dropped after invalidation");
  // Repeat on an actual separate JS thread while the application thread owns
  // socket events and deferred QObject deletion. No runtime access leaves JS.
  std::atomic_bool finished{false};
  std::thread jsThread([&] {
    {
      auto jsRuntime = facebook::hermes::makeHermesRuntime();
      auto& js = *jsRuntime;
      auto jsInvoker = std::make_shared<Invoker>();
      auto jsModule = std::make_unique<Module>(jsInvoker);
      eval(js, "globalThis.events = []; globalThis.__rctDeviceEventEmitter = {"
               "emit(name, event) { events.push({name, ...event}); }};");
      call(js, *jsModule, "connect", jsi::String::createFromUtf8(js, url),
           jsi::Value::null(), jsi::Object(js), 30);
      const auto waitJs = [&](int expected) {
        QElapsedTimer timeout;
        timeout.start();
        while (eval(js, "events.length").getNumber() < expected && timeout.elapsed() < 3000) {
          jsInvoker->drain(js);
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(eval(js, "events.length").getNumber() == expected, "cross-thread event count");
      };
      waitJs(1);
      call(js, *jsModule, "send", jsi::String::createFromAscii(js, "threaded"), 30);
      waitJs(2);
      check(eval(js, "events[1].data === 'threaded' && events[1].id === 30").getBool(),
            "cross-thread echo payload");
      call(js, *jsModule, "ping", 998);
      QElapsedTimer pendingTimeout;
      pendingTimeout.start();
      while (!jsInvoker->hasPending() && pendingTimeout.elapsed() < 3000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      check(jsInvoker->hasPending(), "cross-thread callback pending");
      jsModule.reset(); // On JS thread: Qt transport must be deleted on app thread.
      jsInvoker->drain(js);
      check(eval(js, "events.length === 2").getBool(), "cross-thread invalidation");
    }
    finished.store(true);
  });
  QElapsedTimer threadTimeout;
  threadTimeout.start();
  while (!finished.load() && threadTimeout.elapsed() < 10000) {
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QThread::msleep(1);
  }
  check(finished.load(), "JS thread completed");
  jsThread.join();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
#else
  threw = false;
  try {
    call(rt, *module, "connect", jsi::String::createFromAscii(rt, "ws://localhost"),
         jsi::Value::null(), jsi::Object(rt), 1);
  } catch (const jsi::JSError& error) {
    threw = std::string(error.what()).find("RNQNX_ENABLE_WEBSOCKETS=ON") != std::string::npos;
  }
  check(threw, "disabled backend throws an actionable error");
#endif
  std::cout << "RNQNX_WEBSOCKET_JSI_OK dispatch events validation invalidation\n";
}
