#pragma once

#include <ReactCommon/TurboModule.h>

namespace rnqnx::runtime {

/// No-op SegmentFetcher for the QNX platform.
///
/// Code-splitting segments are not supported on this platform. The module is
/// installed so bundles that register a segment fetcher do not fail TurboModule
/// resolution, but `fetchSegment` invokes the callback with an error immediately.
class QnxSegmentFetcher final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "SegmentFetcher";

  explicit QnxSegmentFetcher(
      std::shared_ptr<facebook::react::CallInvoker> jsInvoker);

 private:
  void registerMethods();

  static facebook::jsi::Value fetchSegment(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value getSegment(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
};

} // namespace rnqnx::runtime
