#pragma once

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rnqnx::runtime {

/// Framework-only adapter for RN's generated LinkingManager contract.
/// Owns a single explicitly configured application URL receiver on the Qt owner.
class QnxLinkingModule final
    : public facebook::react::NativeLinkingManagerCxxSpec<QnxLinkingModule> {
 public:
  explicit QnxLinkingModule(std::shared_ptr<facebook::react::CallInvoker> invoker);
  ~QnxLinkingModule() override;

  /// Configure once before evaluating the app. Only custom application schemes
  /// are accepted; common web/file/system schemes need a separate scoped provider.
  /// A second runtime cannot take over an already attached application receiver.
  void configure(const std::vector<std::string>& schemes,
      const std::optional<std::string>& initialURL);
  void invalidate() noexcept;

  facebook::jsi::Value getInitialURL(facebook::jsi::Runtime& runtime);
  facebook::jsi::Value canOpenURL(facebook::jsi::Runtime& runtime, facebook::jsi::String url);
  facebook::jsi::Value openURL(facebook::jsi::Runtime& runtime, facebook::jsi::String url);
  facebook::jsi::Value openSettings(facebook::jsi::Runtime& runtime);
  void addListener(facebook::jsi::Runtime& runtime, facebook::jsi::String event);
  void removeListeners(facebook::jsi::Runtime& runtime, double count);

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
  facebook::jsi::Value unavailable(facebook::jsi::Runtime& runtime, const char* operation);
};

}  // namespace rnqnx::runtime
