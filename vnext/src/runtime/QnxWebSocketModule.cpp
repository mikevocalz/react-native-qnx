#include "react-native-qnx/runtime/QnxWebSocketModule.h"

namespace rnqnx::runtime {

QnxWebSocketModule::QnxWebSocketModule(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker)
    : facebook::react::TurboModule(kModuleName, std::move(jsInvoker)) {
  registerMethods();
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

// The static method dispatchers below ignore their arguments and return
// undefined. They exist only to satisfy TurboModule resolution for bundles that
// reference WebSocketModule. A real QNX implementation would wire Qt
// WebSockets (or an equivalent) into the runtime event loop and emit the
// expected RCTEventEmitter events.

facebook::jsi::Value QnxWebSocketModule::connect(
    facebook::jsi::Runtime& /*rt*/,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* /*args*/,
    size_t /*count*/) {
  return facebook::jsi::Value::undefined();
}

facebook::jsi::Value QnxWebSocketModule::send(
    facebook::jsi::Runtime& /*rt*/,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* /*args*/,
    size_t /*count*/) {
  return facebook::jsi::Value::undefined();
}

facebook::jsi::Value QnxWebSocketModule::sendBinary(
    facebook::jsi::Runtime& /*rt*/,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* /*args*/,
    size_t /*count*/) {
  return facebook::jsi::Value::undefined();
}

facebook::jsi::Value QnxWebSocketModule::ping(
    facebook::jsi::Runtime& /*rt*/,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* /*args*/,
    size_t /*count*/) {
  return facebook::jsi::Value::undefined();
}

facebook::jsi::Value QnxWebSocketModule::close(
    facebook::jsi::Runtime& /*rt*/,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* /*args*/,
    size_t /*count*/) {
  return facebook::jsi::Value::undefined();
}

facebook::jsi::Value QnxWebSocketModule::addListener(
    facebook::jsi::Runtime& /*rt*/,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* /*args*/,
    size_t /*count*/) {
  return facebook::jsi::Value::undefined();
}

facebook::jsi::Value QnxWebSocketModule::removeListeners(
    facebook::jsi::Runtime& /*rt*/,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* /*args*/,
    size_t /*count*/) {
  return facebook::jsi::Value::undefined();
}

} // namespace rnqnx::runtime
