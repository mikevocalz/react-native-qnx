#include "react-native-qnx/runtime/QnxWebSocketModule.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>

#ifdef RNQNX_ENABLE_WEBSOCKETS
#include "react-native-qnx/runtime/QtWebSocketTransport.h"
#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>
#endif

namespace rnqnx::runtime {
namespace jsi = facebook::jsi;
namespace react = facebook::react;

struct QnxWebSocketModule::Impl {
  std::atomic_bool active{true};
#ifdef RNQNX_ENABLE_WEBSOCKETS
  std::shared_ptr<QtWebSocketTransport> transport;
#endif
};

namespace {
void arguments(jsi::Runtime& rt, size_t count, size_t expected) {
  if (count != expected) {
    throw jsi::JSError(rt, "WebSocketModule: invalid argument count");
  }
}
[[maybe_unused]] int socketId(jsi::Runtime& rt, const jsi::Value& value) {
  if (!value.isNumber() || !std::isfinite(value.getNumber()) ||
      value.getNumber() < 0 ||
      value.getNumber() > std::numeric_limits<int>::max() ||
      std::floor(value.getNumber()) != value.getNumber()) {
    throw jsi::JSError(rt, "WebSocketModule: socket ID must be a nonnegative integer");
  }
  return static_cast<int>(value.getNumber());
}
#ifndef RNQNX_ENABLE_WEBSOCKETS
[[noreturn]] void disabled(jsi::Runtime& rt) {
  throw jsi::JSError(rt,
      "WebSocket transport is disabled. Configure RNQNX_ENABLE_WEBSOCKETS=ON "
      "for the experimental host backend; QNX acceptance gates remain unmet.");
}
#else
QString string(jsi::Runtime& rt, const jsi::Value& value) {
  if (!value.isString()) {
    throw jsi::JSError(rt, "WebSocketModule: expected a string");
  }
  return QString::fromStdString(value.getString(rt).utf8(rt));
}

template <typename F>
void enqueue(const std::shared_ptr<QtWebSocketTransport>& transport, F command) {
  std::weak_ptr<QtWebSocketTransport> weak = transport;
  QMetaObject::invokeMethod(transport.get(), [weak, command = std::move(command)] {
    if (auto live = weak.lock()) {
      command(*live);
    }
  }, Qt::QueuedConnection);
}
#endif
} // namespace

QnxWebSocketModule::QnxWebSocketModule(std::shared_ptr<react::CallInvoker> jsInvoker)
    : react::TurboModule(kModuleName, jsInvoker), impl_(std::make_shared<Impl>()) {
#ifdef RNQNX_ENABLE_WEBSOCKETS
  auto* app = QCoreApplication::instance();
  if (!app) {
    throw std::runtime_error("WebSocketModule requires a QCoreApplication event loop");
  }
  std::weak_ptr<Impl> weak = impl_;
  auto emit = [weak, jsInvoker](QtWebSocketTransport::Event event) {
    auto state = weak.lock();
    if (!state || !state->active.load()) {
      return;
    }
    // This is the same device-emitter contract used by TurboModule::emitDeviceEvent,
    // with an extra lifetime guard so queued events are dropped at invalidation.
    jsInvoker->invokeAsync([weak, event = std::move(event)](jsi::Runtime& rt) {
      auto live = weak.lock();
      if (!live || !live->active.load()) {
        return;
      }
      auto emitter = rt.global().getProperty(rt, "__rctDeviceEventEmitter");
      if (!emitter.isObject()) {
        return;
      }
      auto object = emitter.asObject(rt);
      auto payload = jsi::Object(rt);
      payload.setProperty(rt, "id", event.id);
      const char* name = nullptr;
      const auto set = [&](const char* key, const QString& value) {
        payload.setProperty(rt, key, jsi::String::createFromUtf8(rt, value.toStdString()));
      };
      using Kind = QtWebSocketTransport::Event::Kind;
      switch (event.kind) {
        case Kind::Open:
          name = "websocketOpen";
          set("protocol", event.protocol);
          break;
        case Kind::Message:
          name = "websocketMessage";
          set("type", event.binary ? QStringLiteral("binary") : QStringLiteral("text"));
          set("data", event.data);
          break;
        case Kind::Closed:
          name = "websocketClosed";
          payload.setProperty(rt, "code", event.code);
          set("reason", event.data);
          break;
        case Kind::Failed:
          name = "websocketFailed";
          set("message", event.data);
          break;
      }
      object.getPropertyAsFunction(rt, "emit").callWithThis(
          rt, object, jsi::String::createFromAscii(rt, name), std::move(payload));
    });
  };
  impl_->transport = std::shared_ptr<QtWebSocketTransport>(
      new QtWebSocketTransport(std::move(emit)), [](QtWebSocketTransport* transport) {
        if (QThread::currentThread() == transport->thread()) {
          delete transport;
        } else {
          transport->deleteLater();
        }
      });
  impl_->transport->moveToThread(app->thread());
#endif
  registerMethods();
}

QnxWebSocketModule::~QnxWebSocketModule() {
  impl_->active.store(false);
}

void QnxWebSocketModule::registerMethods() {
  methodMap_["connect"] = {4, &QnxWebSocketModule::connect};
  methodMap_["send"] = {2, &QnxWebSocketModule::send};
  methodMap_["sendBinary"] = {2, &QnxWebSocketModule::sendBinary};
  methodMap_["ping"] = {1, &QnxWebSocketModule::ping};
  methodMap_["close"] = {3, &QnxWebSocketModule::close};
  methodMap_["addListener"] = {1, &QnxWebSocketModule::addListener};
  methodMap_["removeListeners"] = {1, &QnxWebSocketModule::removeListeners};
}

jsi::Value QnxWebSocketModule::connect(jsi::Runtime& rt, react::TurboModule& tm,
                                      const jsi::Value* args, size_t count) {
  arguments(rt, count, 4);
#ifdef RNQNX_ENABLE_WEBSOCKETS
  const int id = socketId(rt, args[3]);
  const QUrl url(string(rt, args[0]), QUrl::StrictMode);
  QStringList protocols;
  if (!args[1].isNull() && !args[1].isUndefined()) {
    const auto list = args[1].asObject(rt).asArray(rt);
    for (size_t i = 0; i < list.size(rt); ++i) {
      protocols.append(string(rt, list.getValueAtIndex(rt, i)));
    }
  }
  QMap<QByteArray, QByteArray> headers;
  if (!args[2].isNull() && !args[2].isUndefined()) {
    const auto options = args[2].asObject(rt);
    const auto value = options.getProperty(rt, "headers");
    if (!value.isNull() && !value.isUndefined()) {
      const auto entries = value.asObject(rt);
      const auto names = entries.getPropertyNames(rt);
      for (size_t i = 0; i < names.size(rt); ++i) {
        const auto key = names.getValueAtIndex(rt, i).asString(rt);
        headers.insert(QString::fromStdString(key.utf8(rt)).toUtf8(),
                       string(rt, entries.getProperty(rt, key)).toUtf8());
      }
    }
  }
  enqueue(static_cast<QnxWebSocketModule&>(tm).impl_->transport,
      [id, url, protocols, headers](QtWebSocketTransport& transport) {
        transport.connectSocket(id, url, protocols, headers);
      });
  return jsi::Value::undefined();
#else
  (void)tm; (void)args;
  disabled(rt);
#endif
}

jsi::Value QnxWebSocketModule::send(jsi::Runtime& rt, react::TurboModule& tm,
                                   const jsi::Value* args, size_t count) {
  arguments(rt, count, 2);
#ifdef RNQNX_ENABLE_WEBSOCKETS
  const auto message = string(rt, args[0]);
  const auto id = socketId(rt, args[1]);
  enqueue(static_cast<QnxWebSocketModule&>(tm).impl_->transport,
      [id, message](QtWebSocketTransport& transport) { transport.sendText(id, message); });
  return jsi::Value::undefined();
#else
  (void)tm; (void)args;
  disabled(rt);
#endif
}

jsi::Value QnxWebSocketModule::sendBinary(jsi::Runtime& rt, react::TurboModule& tm,
                                         const jsi::Value* args, size_t count) {
  arguments(rt, count, 2);
#ifdef RNQNX_ENABLE_WEBSOCKETS
  const auto message = string(rt, args[0]).toUtf8();
  const auto id = socketId(rt, args[1]);
  enqueue(static_cast<QnxWebSocketModule&>(tm).impl_->transport,
      [id, message](QtWebSocketTransport& transport) { transport.sendBinary(id, message); });
  return jsi::Value::undefined();
#else
  (void)tm; (void)args;
  disabled(rt);
#endif
}

jsi::Value QnxWebSocketModule::ping(jsi::Runtime& rt, react::TurboModule& tm,
                                   const jsi::Value* args, size_t count) {
  arguments(rt, count, 1);
#ifdef RNQNX_ENABLE_WEBSOCKETS
  const auto id = socketId(rt, args[0]);
  enqueue(static_cast<QnxWebSocketModule&>(tm).impl_->transport,
      [id](QtWebSocketTransport& transport) { transport.ping(id); });
  return jsi::Value::undefined();
#else
  (void)tm; (void)args;
  disabled(rt);
#endif
}

jsi::Value QnxWebSocketModule::close(jsi::Runtime& rt, react::TurboModule& tm,
                                    const jsi::Value* args, size_t count) {
  arguments(rt, count, 3);
#ifdef RNQNX_ENABLE_WEBSOCKETS
  const auto id = socketId(rt, args[2]);
  const auto code = socketId(rt, args[0]);
  const auto reason = string(rt, args[1]);
  enqueue(static_cast<QnxWebSocketModule&>(tm).impl_->transport,
      [id, code, reason](QtWebSocketTransport& transport) { transport.closeSocket(id, code, reason); });
  return jsi::Value::undefined();
#else
  (void)tm; (void)args;
  disabled(rt);
#endif
}

jsi::Value QnxWebSocketModule::addListener(jsi::Runtime& rt, react::TurboModule&,
                                          const jsi::Value*, size_t count) {
  arguments(rt, count, 1);
  // DeviceEventEmitter owns subscriptions; sockets do not depend on listener counts.
  return jsi::Value::undefined();
}
jsi::Value QnxWebSocketModule::removeListeners(jsi::Runtime& rt, react::TurboModule&,
                                              const jsi::Value*, size_t count) {
  arguments(rt, count, 1);
  return jsi::Value::undefined();
}
} // namespace rnqnx::runtime
