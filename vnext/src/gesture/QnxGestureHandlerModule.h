#pragma once

#include "react-native-qnx/gesture/GestureHandlerCore.h"

#include <ReactCommon/TurboModule.h>

#include <functional>

namespace rnqnx::gesture {

/// `RNGestureHandlerModule`, the TurboModule Gesture Handler's JavaScript uses
/// (src/specs/NativeRNGestureHandlerModule.ts). It drives the recognizer
/// registry. Operations apply immediately, so `flushOperations` has no queue to
/// flush.
class QnxGestureHandlerModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "RNGestureHandlerModule";

  struct Hooks {
    std::function<void(const std::string&, int, const Config&)> create;
    std::function<void(int, int, int)> attach;
    std::function<void(int, const Config&)> setConfig;
    std::function<void(int, const Relations&)> relations;
    std::function<void(int)> drop;
    /// Installs `_setGestureStateAsync` on the RN runtime and
    /// `_setGestureStateSync` on the UI runtime.
    std::function<bool(facebook::jsi::Runtime&)> installBindings;
  };

  QnxGestureHandlerModule(std::shared_ptr<facebook::react::CallInvoker> jsInvoker, Hooks hooks);

 private:
  static facebook::jsi::Value createGestureHandler(facebook::jsi::Runtime&, facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value attachGestureHandler(facebook::jsi::Runtime&, facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value setGestureHandlerConfig(facebook::jsi::Runtime&, facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value configureRelations(facebook::jsi::Runtime&, facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value dropGestureHandler(facebook::jsi::Runtime&, facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value flushOperations(facebook::jsi::Runtime&, facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value installUIRuntimeBindings(facebook::jsi::Runtime&, facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value getModuleId(facebook::jsi::Runtime&, facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);

  Hooks hooks_;
};

}  // namespace rnqnx::gesture
