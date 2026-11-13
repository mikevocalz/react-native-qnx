#pragma once

#include <ReactCommon/TurboModule.h>
#include "react-native-qnx/runtime/QtImageCache.h"

namespace rnqnx::runtime {

/// React Native's `ImageLoader` TurboModule. The upstream version is
/// iOS/Android-only, so this provides `getSize`, `prefetchImage`, and the
/// minimum surface area `Image.getSize` / `Image.prefetch` need.
class QnxImageLoader final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "ImageLoader";

  explicit QnxImageLoader(std::shared_ptr<facebook::react::CallInvoker> jsInvoker);

 private:
  QtImageCache cache_;
  void registerMethods();
  static facebook::jsi::Value load(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count, bool headers,
      bool prefetch);

  static facebook::jsi::Value getSize(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value getSizeWithHeaders(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value prefetchImage(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value prefetchImageWithMetadata(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value queryCache(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
  static facebook::jsi::Value prefetchImageWithOptions(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value clearMemoryCache(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value clearDiskCache(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value getCachePath(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value getConstants(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
};

}  // namespace rnqnx::runtime
