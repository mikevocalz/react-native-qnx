// ExpoImage for QNX: the native module expo-image 58 requires at import time,
// and the view config its requireNativeView('ExpoImage') reads.
//
// The view is drawn by the renderer's Fabric component
// ViewManagerAdapter_ExpoImage (RNQNX_ENABLE_QT_EXPO_IMAGE), on the same Qt
// image loader and decoded cache as React Native's own Image. The module's
// cache functions forward to the renderer's ImageLoader module.
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

jsi::Value resolved(jsi::Runtime& runtime, jsi::Value value) {
  auto promise = runtime.global().getPropertyAsFunction(runtime, "Promise");
  return promise.getPropertyAsFunction(runtime, "resolve")
      .callWithThis(runtime, promise, std::move(value));
}

jsi::Function function(jsi::Runtime& runtime, const char* name, unsigned int arguments,
                       jsi::HostFunctionType body) {
  return jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forAscii(runtime, name), arguments, std::move(body));
}

// The renderer's ImageLoader TurboModule, or an error when this host has none.
jsi::Object imageLoader(jsi::Runtime& runtime) {
  auto proxy = runtime.global().getProperty(runtime, "__turboModuleProxy");
  if (!proxy.isObject() || !proxy.asObject(runtime).isFunction(runtime)) {
    throw jsi::JSError(runtime, "expo-image needs the ImageLoader module, which this host lacks");
  }
  auto loader = proxy.asObject(runtime).asFunction(runtime).call(
      runtime, jsi::String::createFromAscii(runtime, "ImageLoader"));
  if (!loader.isObject()) throw jsi::JSError(runtime, "ImageLoader module is unavailable");
  return loader.asObject(runtime);
}

jsi::Value callImageLoader(jsi::Runtime& runtime, const char* method, std::vector<jsi::Value> args) {
  auto loader = imageLoader(runtime);
  auto function = loader.getPropertyAsFunction(runtime, method);
  return function.callWithThis(runtime, loader, static_cast<const jsi::Value*>(args.data()), args.size());
}

// expo-image's prefetch(urls, cachePolicy, headers): true when every URL is
// fetched, false when any fails (it never rejects).
jsi::Value prefetchAll(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 1 || !args[0].isObject() || !args[0].asObject(runtime).isArray(runtime)) {
    return resolved(runtime, jsi::Value(false));
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
  auto yes = function(runtime, "prefetched", 0, [](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
    return jsi::Value(true);
  });
  auto no = function(runtime, "prefetchFailed", 0, [](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
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
      function(runtime, "prefetch", 3, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        return prefetchAll(rt, args, count);
      }));
  for (const char* name : {"clearMemoryCache", "clearDiskCache"}) {
    module.setProperty(runtime, name,
        function(runtime, name, 0, [name](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
          return callImageLoader(rt, name, {});
        }));
  }
  module.setProperty(runtime, "getCachePathAsync",
      function(runtime, "getCachePathAsync", 1, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        if (count < 1) return resolved(rt, jsi::Value::null());
        std::vector<jsi::Value> forwarded;
        forwarded.emplace_back(rt, args[0]);
        return callImageLoader(rt, "getCachePath", std::move(forwarded));
      }));
  module.setProperty(runtime, "configureCache",
      function(runtime, "configureCache", 1, [](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
        return jsi::Value::undefined();
      }));
  // Blurhash, thumbhash, loadAsync and the cache read/write functions need
  // native decoders and are absent: calling one fails with "is not a function"
  // instead of returning invented data.

  // Functions expo-image calls on the view ref. The view shows no animated
  // images and holds no resource lock, so these have nothing to do.
  jsi::Object prototypes(runtime);
  jsi::Object imageView(runtime);
  for (const char* name : {"startAnimating", "stopAnimating"}) {
    imageView.setProperty(runtime, name,
        function(runtime, name, 0, [](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
          return jsi::Value::undefined();
        }));
  }
  for (const char* name : {"lockResourceAsync", "unlockResourceAsync", "reloadAsync"}) {
    imageView.setProperty(runtime, name,
        function(runtime, name, 0, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
          return resolved(rt, jsi::Value::undefined());
        }));
  }
  prototypes.setProperty(runtime, "ExpoImage", std::move(imageView));
  module.setProperty(runtime, "ViewPrototypes", std::move(prototypes));
  return module;
}

} // namespace

void registerModule() {
  auto& registry = ExpoQnxModuleRegistry::instance();
  registry.registerModule("ExpoImage", createModule);
  registry.registerViewConfig("ExpoImage", "", viewConfig());
}

} // namespace rnqnx::expo::image
