#pragma once

#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/worklets/QtWorkletsPlatform.h"

#include <ReactCommon/TurboModule.h>
#include <reanimated/NativeModules/ReanimatedModuleProxy.h>

#include <memory>

namespace rnqnx::reanimated {

/// `ReanimatedModule`, the TurboModule Reanimated's JavaScript looks up
/// (src/specs/NativeReanimatedModule.ts). Its one method builds the proxy.
class QnxReanimatedModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "ReanimatedModule";

  QnxReanimatedModule(
      std::shared_ptr<facebook::react::CallInvoker> jsInvoker,
      runtime::QtReactHost& host,
      std::shared_ptr<worklets::QtWorkletsPlatform> platform);

  /// Owner thread, while the host's runtime is alive.
  void invalidate();

 private:
  static facebook::jsi::Value installTurboModule(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);

  void install(facebook::jsi::Runtime& rt);
  ::reanimated::PlatformDepMethodsHolder platformMethods();

  runtime::QtReactHost& host_;
  std::shared_ptr<worklets::QtWorkletsPlatform> platform_;
  std::shared_ptr<::reanimated::ReanimatedModuleProxy> proxy_;
  std::shared_ptr<::worklets::WorkletRuntime> uiRuntime_;
  bool invalidated_{false};
};

}  // namespace rnqnx::reanimated
