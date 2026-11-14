#pragma once

#include <ReactCommon/TurboModule.h>
#include "react-native-qnx/runtime/QtImageCache.h"
#include "react-native-qnx/views/image/ImageLoadedNotifier.h"
#include "react-native-qnx/views/image/QtImageFetcher.h"

namespace rnqnx::runtime {

/// Calls `emit('imageLoaded', { url, width, height })` on the expo-image module
/// when one is installed and has a listener (QnxImageLoaderRefs.cpp).
void emitImageLoaded(facebook::jsi::Runtime& runtime, const views::ImageLoadedEvent& event);

/// React Native's `ImageLoader` TurboModule. The upstream version is
/// iOS/Android-only, so this provides `getSize`, `prefetchImage`, and the
/// minimum surface area `Image.getSize` / `Image.prefetch` need. It also serves
/// @rn-qnx/expo-image-qnx: the cache controls and the ImageRef functions
/// (`loadImage`, `generateBlurhash`, `writeToCache`, ...), which live in
/// QnxImageLoaderRefs.cpp.
class QnxImageLoader final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "ImageLoader";

  explicit QnxImageLoader(std::shared_ptr<facebook::react::CallInvoker> jsInvoker);
  ~QnxImageLoader() override;

 private:
  QtImageCache cache_;
  views::QtImageFetcher fetcher_{30000, &views::ImageDiskCache::shared(), &views::ImageFetchQueue::shared()};
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
  static facebook::jsi::Value configureCache(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value getCachePath(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  // ImageRef functions for expo-image (QnxImageLoaderRefs.cpp). A "source" is
  // an ImageRef id (number), a URI string or an expo-image source object.
  static facebook::jsi::Value loadImage(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value releaseImage(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value generateBlurhash(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value generateThumbhash(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value writeToCache(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value readFromCache(
      facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args, size_t count);
  static facebook::jsi::Value getConstants(
      facebook::jsi::Runtime& rt,
      facebook::react::TurboModule& tm,
      const facebook::jsi::Value* args,
      size_t count);
};

}  // namespace rnqnx::runtime
