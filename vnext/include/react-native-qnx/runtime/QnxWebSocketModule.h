#pragma once

#include <ReactCommon/TurboModule.h>

namespace rnqnx::runtime {

/// No-op WebSocketModule for the QNX platform.
///
/// React Native's WebSocket implementation is not wired to a native socket
/// backend on this platform. The module is installed so that bundles that merely
/// reference it do not crash on TurboModule resolution, but every operation is
/// a no-op or warning. A real implementation would need Qt WebSockets or an
/// equivalent QNX-native async socket library integrated with the runtime's
/// event loop.
class QnxWebSocketModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "WebSocketModule";

  explicit QnxWebSocketModule(
      std::shared_ptr<facebook::react::CallInvoker> jsInvoker);

 private:
  void registerMethods();

  static facebook::jsi::Value connect(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value send(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value sendBinary(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value ping(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value close(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value addListener(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value removeListeners(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
};

} // namespace rnqnx::runtime
