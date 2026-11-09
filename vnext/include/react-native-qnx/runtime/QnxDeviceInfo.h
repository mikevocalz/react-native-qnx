#pragma once

#include "react-native-qnx/runtime/WindowMetrics.h"

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>

#include <memory>

namespace rnqnx::runtime {

/// React Native's existing internal DeviceInfo contract. Constants and live
/// didUpdateDimensions events use the same logical-unit snapshot. There is one
/// instance per host/runtime; RN owns additive JavaScript subscriptions.
class QnxDeviceInfo final
    : public facebook::react::NativeDeviceInfoCxxSpec<QnxDeviceInfo> {
 public:
  QnxDeviceInfo(std::shared_ptr<facebook::react::CallInvoker> jsInvoker,
                const WindowMetrics& initial);
  ~QnxDeviceInfo() override;

  facebook::jsi::Object getConstants(facebook::jsi::Runtime& runtime);
  void updateMetrics(const WindowMetrics& metrics);
  void invalidate() noexcept;

 private:
  struct State;
  std::shared_ptr<State> state_;
  std::shared_ptr<facebook::react::CallInvoker> invoker_;
};

}  // namespace rnqnx::runtime
