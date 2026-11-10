#pragma once

#include <ReactCommon/TurboModule.h>
#include <memory>

namespace rnqnx::fabric { class ShadowViewMutationAdapter; }

namespace rnqnx::runtime {

/// Internal adapter for React Native AccessibilityInfo. Uses the Qt owner and
/// existing RuntimeScheduler; no application-facing TurboModule API is added.
class QtAccessibilityInfoModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "QtAccessibilityInfo";
  QtAccessibilityInfoModule(std::shared_ptr<facebook::react::CallInvoker> invoker,
                            fabric::ShadowViewMutationAdapter& adapter);
  ~QtAccessibilityInfoModule() override;
  void invalidate() noexcept;

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
  static facebook::jsi::Value isScreenReaderEnabled(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value isHighTextContrastEnabled(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value setAccessibilityFocus(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value announceForAccessibility(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
};

}  // namespace rnqnx::runtime
