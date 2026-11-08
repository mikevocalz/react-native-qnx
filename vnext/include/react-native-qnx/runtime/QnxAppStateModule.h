#pragma once

#include <ReactCommon/TurboModule.h>
#include <memory>

namespace rnqnx::runtime {

/// React Native's own AppState adapter, not an application-facing native API.
/// Construct, invoke and invalidate on the QGuiApplication/runtime owner thread.
class QnxAppStateModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "AppState";
  explicit QnxAppStateModule(std::shared_ptr<facebook::react::CallInvoker> jsInvoker);
  ~QnxAppStateModule() override;

  /// Stop the Qt producer and invalidate accepted callbacks before Hermes dies.
  void invalidate() noexcept;

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;

  static facebook::jsi::Value getConstants(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value getCurrentAppState(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value addListener(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value removeListeners(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
};

}  // namespace rnqnx::runtime
