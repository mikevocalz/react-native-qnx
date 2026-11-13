#pragma once

#include <ReactCommon/TurboModule.h>
#include <worklets/NativeModules/WorkletsModuleProxy.h>
#include <worklets/NativeModules/WorkletsModuleProxyInitializer.h>

#include <functional>
#include <memory>

namespace rnqnx::worklets {

/// The `WorkletsModule` that react-native-worklets' JavaScript looks up with
/// `TurboModuleRegistry.get('WorkletsModule')`
/// (src/specs/NativeWorkletsModule.ts: installTurboModule, prepareBundleMode,
/// toggleSlowAnimationsOnUIRuntime, start).
///
/// This is the third-party bootstrap exception recorded in
/// docs/adr/0020-worklets-reanimated-gesture-handler-bootstrap.md: the library
/// owns its JSI proxy and installs it on the RN runtime itself.
class QnxWorkletsModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "WorkletsModule";

  using UiRuntimeReady = std::function<void(facebook::jsi::Runtime& uiRuntime)>;

  QnxWorkletsModule(
      std::shared_ptr<facebook::react::CallInvoker> jsInvoker,
      std::shared_ptr<::worklets::WorkletsModuleProxyInitializer> initializer,
      UiRuntimeReady onUiRuntimeReady);

  /// Owner thread. Drops the proxy and stops accepting JS calls.
  void invalidate();

 private:
  static facebook::jsi::Value installTurboModule(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value prepareBundleMode(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value toggleSlowAnimationsOnUIRuntime(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value start(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);

  std::shared_ptr<::worklets::WorkletsModuleProxyInitializer> initializer_;
  std::shared_ptr<::worklets::WorkletsModuleProxy> proxy_;
  UiRuntimeReady onUiRuntimeReady_;
  bool invalidated_{false};
};

}  // namespace rnqnx::worklets
