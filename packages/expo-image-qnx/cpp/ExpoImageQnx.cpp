// ExpoImage for QNX: the native module expo-image 58 requires at import time,
// and the view config its requireNativeView('ExpoImage') reads.
//
// The view is drawn by the renderer's Fabric component
// ViewManagerAdapter_ExpoImage (RNQNX_ENABLE_QT_EXPO_IMAGE), on the same Qt
// image loader and decoded cache as React Native's own Image. The module's
// cache functions forward to the renderer's ImageLoader module.
#include "ExpoImageSupport.hpp"
#include "ExpoQnxModuleRegistry.hpp"

#include <jsi/jsi.h>

#include <string>
#include <utility>
#include <vector>

namespace rnqnx::expo::image {
namespace {

namespace jsi = facebook::jsi;

// The props expo-image's ExpoImage.tsx passes to the native view: its own
// (ImageNativeProps) and the style and color values it spreads onto the view.
ExpoViewConfig viewConfig() {
  ExpoViewConfig config;
  config.props = {
      "source",
      "placeholder",
      "placeholderContentFit",
      "responsivePolicy",
      "contentFit",
      "contentPosition",
      "transition",
      "tintColor",
      "blurRadius",
      "priority",
      "cachePolicy",
      "recyclingKey",
      "allowDownscaling",
      "autoplay",
      "enableLiveTextInteraction",
      "accessibilityLabel",
      "accessibilityRole",
      "accessibilityHint",
      "accessibilityState",
      "accessibilityValue",
      "accessible",
      "focusable",
      "testID",
      "nativeID",
      "pointerEvents",
      "hitSlop",
      "opacity",
      "transform",
      "borderRadius",
      "borderTopLeftRadius",
      "borderTopRightRadius",
      "borderBottomRightRadius",
      "borderBottomLeftRadius",
      "borderWidth",
      "borderColor",
      "borderTopColor",
      "borderRightColor",
      "borderBottomColor",
      "borderLeftColor",
      "borderStartColor",
      "borderEndColor",
      "backgroundColor",
  };
  config.events = {"onLoadStart", "onProgress", "onLoad", "onError", "onDisplay", "onLayout"};
  return config;
}

// expo-image's prefetch(urls, cachePolicy, headers): true when every URL is
// fetched, false when any fails (it never rejects).
jsi::Value prefetchAll(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 1 || !args[0].isObject() || !args[0].asObject(runtime).isArray(runtime)) {
    return resolvedPromise(runtime, jsi::Value(false));
  }
  auto urls = args[0].asObject(runtime).asArray(runtime);
  jsi::Array pending(runtime, urls.size(runtime));
  for (size_t i = 0; i < urls.size(runtime); ++i) {
    jsi::Object options(runtime);
    if (count > 1 && args[1].isString()) options.setProperty(runtime, "cachePolicy", args[1]);
    if (count > 2 && args[2].isObject()) options.setProperty(runtime, "headers", args[2]);
    std::vector<jsi::Value> forwarded;
    forwarded.push_back(urls.getValueAtIndex(runtime, i));
    forwarded.emplace_back(std::move(options));
    pending.setValueAtIndex(runtime, i, callImageLoader(runtime, "prefetchImageWithOptions", std::move(forwarded)));
  }
  auto promise = runtime.global().getPropertyAsFunction(runtime, "Promise");
  auto all = promise.getPropertyAsFunction(runtime, "all").callWithThis(runtime, promise, std::move(pending));
  auto then = all.asObject(runtime).getPropertyAsFunction(runtime, "then");
  auto yes = hostFunction(runtime, "prefetched", 0, [](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
    return jsi::Value(true);
  });
  auto no = hostFunction(runtime, "prefetchFailed", 0, [](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
    return jsi::Value(false);
  });
  return then.callWithThis(runtime, all.asObject(runtime), std::move(yes), std::move(no));
}

jsi::Object createModule(jsi::Runtime& runtime) {
  jsi::Object module(runtime);
  module.setProperty(runtime, "name", "ExpoImage");

  // Cache controls. The renderer's ImageLoader owns the decoded cache and the
  // disk cache; these forward to it through the TurboModule proxy, so they act
  // on exactly what the view reads.
  module.setProperty(runtime, "prefetch",
      hostFunction(runtime, "prefetch", 3, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        return prefetchAll(rt, args, count);
      }));
  for (const char* name : {"clearMemoryCache", "clearDiskCache"}) {
    module.setProperty(runtime, name,
        hostFunction(runtime, name, 0, [name](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
          return callImageLoader(rt, name, {});
        }));
  }
  module.setProperty(runtime, "getCachePathAsync",
      hostFunction(runtime, "getCachePathAsync", 1, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        if (count < 1) return resolvedPromise(rt, jsi::Value::null());
        std::vector<jsi::Value> forwarded;
        forwarded.emplace_back(rt, args[0]);
        return callImageLoader(rt, "getCachePath", std::move(forwarded));
      }));
  module.setProperty(runtime, "configureCache",
      hostFunction(runtime, "configureCache", 1, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        std::vector<jsi::Value> forwarded;
        forwarded.push_back(count > 0 ? jsi::Value(rt, args[0]) : jsi::Value::undefined());
        callImageLoader(rt, "configureCache", std::move(forwarded));
        return jsi::Value::undefined();
      }));

  // loadAsync, the hash generators, the cache read and write functions and the
  // ImageRef class.
  installImageRefs(runtime, module);
  // startAnimating, stopAnimating and the resource functions of a view ref.
  installViewPrototypes(runtime, module);
  return module;
}

} // namespace

void registerModule() {
  auto& registry = ExpoQnxModuleRegistry::instance();
  registry.registerModule("ExpoImage", createModule);
  registry.registerViewConfig("ExpoImage", "", viewConfig());
}

} // namespace rnqnx::expo::image
