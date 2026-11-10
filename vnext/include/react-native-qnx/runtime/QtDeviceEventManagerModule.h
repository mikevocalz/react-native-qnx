#pragma once

#include <ReactCommon/TurboModule.h>
#include <memory>

namespace rnqnx::fabric { class ShadowViewMutationAdapter; }

namespace rnqnx::runtime {

/// Window-scoped backend for RN's own DeviceEventManager/BackHandler contract.
class QtDeviceEventManagerModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "DeviceEventManager";
  QtDeviceEventManagerModule(std::shared_ptr<facebook::react::CallInvoker> invoker,
                             fabric::ShadowViewMutationAdapter& adapter);
  ~QtDeviceEventManagerModule() override;
  void invalidate() noexcept;

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
  static facebook::jsi::Value invokeDefaultBackPressHandler(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
};

}  // namespace rnqnx::runtime
