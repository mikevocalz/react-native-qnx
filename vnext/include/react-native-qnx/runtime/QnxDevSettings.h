#pragma once

#include <ReactCommon/TurboModule.h>
#include <functional>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>

namespace rnqnx::runtime {

// Internal RN integration, not an application-facing Nitro API.
class QnxDevSettings final : public facebook::react::TurboModule {
 public:
  using Reload = std::function<void(std::string)>;
  using Report = std::function<void(std::string, std::string)>;
  static constexpr const char* kModuleName = "DevSettings";
  QnxDevSettings(std::shared_ptr<facebook::react::CallInvoker> invoker,
                 Reload reload, Report report);

 private:
  static facebook::jsi::Value reload(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value refreshed(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value unsupported(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  Reload reload_;
  Report report_;
};

void installDevGlobals(facebook::jsi::Runtime& runtime,
    const std::string& bundleUrl, std::uint64_t generation,
    QnxDevSettings::Report report);

} // namespace rnqnx::runtime
