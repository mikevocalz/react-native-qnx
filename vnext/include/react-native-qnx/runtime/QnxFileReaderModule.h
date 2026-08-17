#pragma once

#include <ReactCommon/TurboModule.h>

namespace rnqnx::runtime {

/// No-op FileReaderModule for the QNX platform.
///
/// React Native's Blob/FileReader API is not implemented on this platform. The
/// module resolves TurboModule lookup so bundles that reference it do not crash,
/// but every call rejects with an error explaining the limitation.
class QnxFileReaderModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "FileReaderModule";

  explicit QnxFileReaderModule(
      std::shared_ptr<facebook::react::CallInvoker> jsInvoker);

 private:
  void registerMethods();

  static facebook::jsi::Value readAsDataURL(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value readAsText(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
};

} // namespace rnqnx::runtime
