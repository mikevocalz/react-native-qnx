#pragma once

#include <ReactCommon/TurboModule.h>

namespace rnqnx::runtime {

/// React Native's WebSocketModule, backed by an explicitly opt-in Qt transport.
/// With the feature disabled, socket operations throw an actionable error.
class QnxWebSocketModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "WebSocketModule";

  explicit QnxWebSocketModule(
      std::shared_ptr<facebook::react::CallInvoker> jsInvoker);
  ~QnxWebSocketModule() override;

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
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
